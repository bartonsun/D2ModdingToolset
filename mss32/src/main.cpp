/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2020 Vladimir Makeev.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma comment(lib, "Lib/detours.lib")

#include "buildstamp.h"
#include "customaibattle.h"
#include "customattacks.h"
#include "custommodifiers.h"
#include "hooks.h"
#include "restrictions.h"
#include "settings.h"
#include "unitsforhire.h"
#include "utils.h"
#include "version.h"
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <cstdint>
#include <string>
#include <thread>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <detours.h>

HMODULE library{};
static HMODULE libraryMss23{};
static void* registerInterface{};
static void* unregisterInterface{};
std::thread::id mainThreadId;

extern "C" __declspec(naked) void __stdcall RIB_register_interface(void)
{
    __asm {
        jmp registerInterface;
    }
}

extern "C" __declspec(naked) void __stdcall RIB_unregister_interface(void)
{
    __asm {
        jmp unregisterInterface;
    }
}

template <typename T>
static void writeProtectedMemory(T* address, T value)
{
    DWORD oldProtection{};
    if (VirtualProtect(address, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtection)) {
        *address = value;
        VirtualProtect(address, sizeof(T), oldProtection, &oldProtection);
        return;
    }

    spdlog::error("Failed to change memory protection for {:p}", (void*)address);
}

template <typename T>
static void adjustRestrictionMax(game::Restriction<T>* restriction,
                                 const T& value,
                                 const char* name)
{
    if (value >= restriction->min) {
        spdlog::debug("Set '{:s}' to {:d}", name, value);
        writeProtectedMemory(&restriction->max, value);
        return;
    }

    spdlog::error(
        "User specified '{:s}' value of {:d} is less than minimum value allowed in game ({:d}). Change rejected.",
        name, value, restriction->min);
}

static void adjustGameRestrictions()
{
    using namespace hooks;

    auto& restrictions = game::gameRestrictions();
    // Allow game to load and scenario editor to create scenarios with maximum allowed spells level
    // set to zero, disabling usage of magic in scenario
    writeProtectedMemory(&restrictions.spellLevel->min, 0);
    // Allow using units with tier higher than 5
    writeProtectedMemory(&restrictions.unitTier->max, 10);

    if (gameSettings().unitMaxDamage != baseGameSettings().unitMaxDamage) {
        adjustRestrictionMax(restrictions.attackDamage, gameSettings().unitMaxDamage,
                             "UnitMaxDamage");
    }

    if (gameSettings().unitMaxArmor != baseGameSettings().unitMaxArmor) {
        adjustRestrictionMax(restrictions.unitArmor, gameSettings().unitMaxArmor, "UnitMaxArmor");
    }

    if (gameSettings().stackScoutRangeMax != baseGameSettings().stackScoutRangeMax) {
        adjustRestrictionMax(restrictions.stackScoutRange, gameSettings().stackScoutRangeMax,
                             "StackMaxScoutRange");
    }

    if (executableIsGame()) {
        if (gameSettings().criticalHitDamage != baseGameSettings().criticalHitDamage) {
            spdlog::debug("Set 'criticalHitDamage' to {:d}", (int)gameSettings().criticalHitDamage);
            writeProtectedMemory(restrictions.criticalHitDamage, gameSettings().criticalHitDamage);
        }

        if (gameSettings().mageLeaderAttackPowerReduction
            != baseGameSettings().mageLeaderAttackPowerReduction) {
            spdlog::debug("Set 'mageLeaderPowerReduction' to {:d}",
                          (int)gameSettings().mageLeaderAttackPowerReduction);
            writeProtectedMemory(restrictions.mageLeaderAttackPowerReduction,
                                 gameSettings().mageLeaderAttackPowerReduction);
        }
    }
}

static bool setupHook(hooks::HookInfo& hook)
{
    spdlog::debug("Try to attach hook. Function {:p}, hook {:p}.", hook.target, hook.hook);

    // hook.original is an optional field that can point to where the new address of the original
    // function should be placed.
    void** pointer = hook.original ? hook.original : (void**)&hook.original;
    *pointer = hook.target;

    auto result = DetourAttach(pointer, hook.hook);
    if (result != NO_ERROR) {
        hooks::showErrorMessageBox(
            fmt::format("Failed to attach hook. Function {:p}, hook {:p}. Error code: {:d}.",
                        hook.target, hook.hook, result));
        return false;
    }

    return true;
}

static bool setupHooks()
{
    auto hooks{hooks::getHooks()};

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    for (auto& hook : hooks) {
        if (!setupHook(hook)) {
            return false;
        }
    }

    const auto result = DetourTransactionCommit();
    if (result != NO_ERROR) {
        hooks::showErrorMessageBox(
            fmt::format("Failed to commit detour transaction. Error code: {:d}.", result));
        return false;
    }

    spdlog::debug("All hooks are set");
    return true;
}

static void setupVftableHooks()
{
    for (const auto& hook : hooks::getVftableHooks()) {
        void** target = (void**)hook.target;
        if (hook.original)
            *hook.original = *target;

        writeProtectedMemory(target, hook.hook);
    }

    spdlog::debug("All vftable hooks are set");
}

static LPTOP_LEVEL_EXCEPTION_FILTER previousExceptionFilter = nullptr;
// A vectored handler sees *every* exception the process raises, including the
// ones the game handles and survives -- SmartHeap raises a read fault on a
// guard page and keeps going. Measured 2026-08-28: one such fault at 14:11:41
// spent the single crash slot, and the fault that actually killed the game 70
// seconds later wrote nothing. First-chance records are bounded on their own
// counter; the terminal filter always gets to write.
static LONG firstChanceLogged = 0;
static LONG terminalLogged = 0;
static const LONG maxFirstChanceRecords = 3;

static bool isFatalExceptionCode(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
        return true;
    default:
        return false;
    }
}

/**
 * A crash leaves the log looking exactly like a game that was closed by hand:
 * the last line written is simply the last line. This writes the faulting
 * address as module + offset, which is the only form comparable between runs,
 * plus the operation and address for an access violation.
 */
static void logCrash(EXCEPTION_POINTERS* info, bool firstChance)
{
    if (!info || !info->ExceptionRecord) {
        return;
    }
    if (firstChance) {
        if (InterlockedIncrement(&firstChanceLogged) > maxFirstChanceRecords) {
            return;
        }
    } else if (InterlockedCompareExchange(&terminalLogged, 1, 0) != 0) {
        return;
    }
    const char* const kind = firstChance ? "first-chance" : "fatal";

    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    const void* address = record->ExceptionAddress;

    std::string module = "unknown";
    std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(address);
    HMODULE handle = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(address), &handle)
        && handle) {
        char path[MAX_PATH]{};
        if (GetModuleFileNameA(handle, path, MAX_PATH)) {
            const std::string full{path};
            const auto slash = full.find_last_of("\\/");
            module = slash == std::string::npos ? full : full.substr(slash + 1);
        }
        offset -= reinterpret_cast<std::uintptr_t>(handle);
    }

    // Both lines are spelled out instead of interpolating `kind`: the log audit
    // greps for these prefixes, and a marker that only exists after a runtime
    // substitution cannot be checked against the source that writes it.
    if (firstChance) {
        spdlog::error("crash first-chance code={:#x} at {}+{:#x} thread={}",
                      record->ExceptionCode, module, offset, GetCurrentThreadId());
    } else {
        spdlog::error("crash fatal code={:#x} at {}+{:#x} thread={}", record->ExceptionCode,
                      module, offset, GetCurrentThreadId());
    }

    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        && record->NumberParameters >= 2) {
        const ULONG_PTR operation = record->ExceptionInformation[0];
        const char* access = operation == 0 ? "read" : (operation == 1 ? "write" : "execute");
        spdlog::error("crash {} access {} at {:#x}", kind, access,
                      static_cast<std::uintptr_t>(record->ExceptionInformation[1]));
    }

    if (info->ContextRecord) {
        const CONTEXT* context = info->ContextRecord;
        spdlog::error("crash {} eip={:#x} esp={:#x} ebp={:#x}", kind,
                      static_cast<std::uintptr_t>(context->Eip),
                      static_cast<std::uintptr_t>(context->Esp),
                      static_cast<std::uintptr_t>(context->Ebp));
    }

    spdlog::default_logger()->flush();
}

static LONG WINAPI unhandledExceptionHooked(EXCEPTION_POINTERS* info)
{
    // This one runs when nothing else claimed the exception, i.e. the game is
    // going down. It writes regardless of how many first-chance records the
    // vectored handler already produced.
    logCrash(info, false);
    return previousExceptionFilter ? previousExceptionFilter(info) : EXCEPTION_CONTINUE_SEARCH;
}

static LONG CALLBACK vectoredExceptionHooked(EXCEPTION_POINTERS* info)
{
    // The game installs its own top-level filter, so a fatal fault can reach a
    // handler that never writes anything. Vectored handlers run first, and this
    // one only reads: the exception keeps travelling untouched.
    if (info && info->ExceptionRecord && isFatalExceptionCode(info->ExceptionRecord->ExceptionCode)) {
        logCrash(info, true);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void setupCrashLogging()
{
    AddVectoredExceptionHandler(1, vectoredExceptionHooked);
    previousExceptionFilter = SetUnhandledExceptionFilter(unhandledExceptionHooked);
}

static void setupDefaultLogger()
{
    auto logger = spdlog::default_logger();
    auto& sinks = logger->sinks();
    sinks.clear();

    // Original mss32.dll has no log file so we are free to use short name "mss32.log"
    // (instead of the old "mss32Proxy.log")
    auto fileName = hooks::gameFolder() / "mss32.log";
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(fileName.string(),
                                                                           5u << 20, 3);
    // TODO: setting for all available trace levels (not only debug vs non-debug)
    fileSink->set_level(hooks::gameSettings().debugMode ? spdlog::level::trace
                                                        : spdlog::level::info);
    sinks.push_back(std::move(fileSink));

    if (IsDebuggerPresent()) {
        auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);
        msvcSink->set_level(spdlog::level::trace);
        sinks.push_back(std::move(msvcSink));
    }

    // Setting maximum log level for the logger so only the sinks levels will matter
    logger->set_level(spdlog::level::trace);
    // Using UTC helps to match logs from different users (with different timezones).
    // It also helps to match client logs with lobby server logs.
    logger->set_pattern("%D %H:%M:%S.%e %5t [%=8!n] [%L] %v", spdlog::pattern_time_type::utc);

    // First line of every run, before any game code: a log from an old dll and a
    // log from a new one are otherwise structurally identical, so a client
    // report of "the fix does not work" cannot be told apart from a file that
    // was never replaced.
    spdlog::info("mss32 build={} logger=ready", hooks::buildStamp);
}

BOOL APIENTRY DllMain(HMODULE hDll, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_DETACH) {
        FreeLibrary(libraryMss23);
        return TRUE;
    }

    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    /*
        https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls#remarks
        Do not call this function from a DLL that is linked to the static C run-time library (CRT).
        The static CRT requires DLL_THREAD_ATTACH and DLL_THREAD_DETATCH notifications to function
        properly.
    */
    // DisableThreadLibraryCalls(hDll);

    setupDefaultLogger();
    setupCrashLogging();

    library = hDll;
    mainThreadId = std::this_thread::get_id();

    libraryMss23 = LoadLibrary("Mss23.dll");
    if (!libraryMss23) {
        hooks::showErrorMessageBox("Failed to load Mss23.dll");
        return FALSE;
    }

    registerInterface = GetProcAddress(libraryMss23, "RIB_register_interface");
    unregisterInterface = GetProcAddress(libraryMss23, "RIB_unregister_interface");
    if (!registerInterface || !unregisterInterface) {
        hooks::showErrorMessageBox("Could not load Mss23.dll addresses");
        return FALSE;
    }

    const auto error = hooks::determineGameVersion(hooks::exePath());
    if (error || hooks::gameVersion() == hooks::GameVersion::Unknown) {
        hooks::showErrorMessageBox(
            fmt::format("Failed to determine target exe type.\nReason: {:s}.", error.message()));
        return FALSE;
    }

    if (hooks::executableIsGame() && !hooks::loadUnitsForHire()) {
        hooks::showErrorMessageBox("Failed to load new units. Check error log for details.");
        return FALSE;
    }

    adjustGameRestrictions();
    setupVftableHooks();
    if (!setupHooks()) {
        return FALSE;
    }

    // Lazy initialization is not optimal as the data can be accessed in parallel threads.
    // Thread sync is excessive because the data is read-only or thread-exclusive once initialized.
    hooks::initializeCustomAttacks();
    hooks::initializeCustomModifiers();
    hooks::initializeCustomAiBattleLogic();
    return TRUE;
}
