#include "dinputdevicehooks.h"
#include "dinputdeviceapi.h"
#include "phasegamehooks.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <detours.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdio>

namespace hooks {
namespace {

using namespace game::DinputDeviceApi;

using GetDeviceStateFn = long(__stdcall*)(void* thisptr, unsigned cbData, void* lpvData);
using GetDeviceDataFn = long(__stdcall*)(void* thisptr,
                                         unsigned cbObjectData,
                                         void* rgdod,
                                         unsigned* pdwInOut,
                                         unsigned flags);
using GetAsyncKeyStateFn = short(__stdcall*)(int vKey);
using GetKeyStateFn = short(__stdcall*)(int nVirtKey);
using GetKeyboardStateFn = int(__stdcall*)(unsigned char* keyState);
using DirectInput8CreateFn = long(__stdcall*)(void* hinst,
                                             unsigned dwVersion,
                                             const Guid* riidltf,
                                             void** ppvOut,
                                             void* punkOuter);
using DirectInputCreateAFn = long(__stdcall*)(void* hinst,
                                             unsigned dwVersion,
                                             void** ppDI,
                                             void* punkOuter);
using DirectInputCreateExFn = long(__stdcall*)(void* hinst,
                                              unsigned dwVersion,
                                              const Guid* riidltf,
                                              void** ppvOut,
                                              void* punkOuter);
using CreateDeviceFn = long(__stdcall*)(void* thisptr, const Guid* rguid, void** out, void* outer);
using ReleaseFn = unsigned long(__stdcall*)(void* thisptr);

GetDeviceStateFn origGetDeviceState = nullptr;
GetDeviceStateFn origGetDeviceStateB = nullptr;
GetDeviceDataFn origGetDeviceData = nullptr;
GetDeviceDataFn origGetDeviceDataB = nullptr;
GetAsyncKeyStateFn origGetAsyncKeyState = nullptr;
GetKeyStateFn origGetKeyState = nullptr;
GetKeyboardStateFn origGetKeyboardState = nullptr;
DirectInputCreateAFn origDirectInputCreateA = nullptr;
DirectInputCreateExFn origDirectInputCreateEx = nullptr;
DirectInput8CreateFn origDirectInput8Create = nullptr;
CreateDeviceFn origCreateDevice = nullptr;

std::atomic<unsigned long> pulseUntil{0};
std::atomic<bool> user32Installed{false};
std::atomic<bool> createExportHooked{false};
std::atomic<bool> createFnAttached{false};
std::atomic<bool> stateHooked{false};
std::atomic<int> stateLogLeft{12};

bool pulseActive()
{
    return mousePulseActive();
}

bool wantLButton()
{
    if (pulseActive()) {
        return true;
    }
    if (origGetAsyncKeyState) {
        return (origGetAsyncKeyState(0x01) & 0x8000) != 0;
    }
    return false;
}

void orLButton(void* lpvData, unsigned cbData)
{
    if (!lpvData || !isMouseStateSize(cbData) || cbData <= kRgbButtonsOffset) {
        return;
    }
    auto* buttons = static_cast<unsigned char*>(lpvData) + kRgbButtonsOffset;
    *buttons |= kButtonDown;
}

bool attachOnce(void** orig, void* hook)
{
    if (!orig || !*orig || !hook) {
        return false;
    }
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    const long att = DetourAttach(orig, hook);
    const long commit = DetourTransactionCommit();
    if (att != NO_ERROR || commit != NO_ERROR) {
        spdlog::error("detour attach {} commit {}", att, commit);
        return false;
    }
    return true;
}

long afterGetDeviceState(long hr, unsigned cbData, void* lpvData)
{
    const bool mouse = isMouseStateSize(cbData);
    const bool inject = mouse && hr >= 0 && wantLButton();
    if (inject) {
        orLButton(lpvData, cbData);
        spdlog::info("dinput GetDeviceState inject LBUTTON cb={}", cbData);
    }
    int left = stateLogLeft.fetch_sub(1);
    if (left > 0) {
        unsigned char b0 = 0;
        if (lpvData && cbData > kRgbButtonsOffset) {
            b0 = *(static_cast<unsigned char*>(lpvData) + kRgbButtonsOffset);
        }
        spdlog::info("dinput GetDeviceState cb={} hr={:x} btn0={:02x} inject={}", cbData,
                     static_cast<unsigned>(hr), b0, inject);
    }
    return hr;
}

long __stdcall getDeviceStateHooked(void* thisptr, unsigned cbData, void* lpvData)
{
    long hr = origGetDeviceState ? origGetDeviceState(thisptr, cbData, lpvData)
                                 : static_cast<long>(0x80004005);
    return afterGetDeviceState(hr, cbData, lpvData);
}

long __stdcall getDeviceStateHookedB(void* thisptr, unsigned cbData, void* lpvData)
{
    long hr = origGetDeviceStateB ? origGetDeviceStateB(thisptr, cbData, lpvData)
                                  : static_cast<long>(0x80004005);
    return afterGetDeviceState(hr, cbData, lpvData);
}

long __stdcall getDeviceDataHooked(void* thisptr,
                                   unsigned cbObjectData,
                                   void* rgdod,
                                   unsigned* pdwInOut,
                                   unsigned flags)
{
    unsigned inMax = pdwInOut ? *pdwInOut : 0;
    long hr = origGetDeviceData ? origGetDeviceData(thisptr, cbObjectData, rgdod, pdwInOut, flags)
                                : static_cast<long>(0x80004005);
    if (hr < 0 || !pdwInOut || !rgdod || cbObjectData < 16) {
        return hr;
    }
    if (!wantLButton()) {
        return hr;
    }
    unsigned n = *pdwInOut;
    if (n >= inMax && inMax != 0) {
        return hr;
    }
    auto* row = static_cast<unsigned char*>(rgdod) + n * cbObjectData;
    for (unsigned i = 0; i < cbObjectData; ++i) {
        row[i] = 0;
    }
    *reinterpret_cast<unsigned*>(row) = kDimofsButton0;
    *reinterpret_cast<unsigned*>(row + 4) = kButtonDown;
    *pdwInOut = n + 1;
    spdlog::info("dinput GetDeviceData inject LBUTTON n={}", n + 1);
    return 0;
}

void hookDeviceMethods(void* dev)
{
    if (!dev) {
        return;
    }
    void** vtbl = *reinterpret_cast<void***>(dev);
    if (!vtbl) {
        return;
    }
    void* st = vtbl[kGetDeviceStateVtbl];
    void* dd = vtbl[kGetDeviceDataVtbl];
    bool ok = true;
    if (st && !origGetDeviceState) {
        origGetDeviceState = reinterpret_cast<GetDeviceStateFn>(st);
        ok = attachOnce(reinterpret_cast<void**>(&origGetDeviceState),
                        reinterpret_cast<void*>(getDeviceStateHooked))
             && ok;
        if (ok) {
            stateHooked.store(true);
        }
    } else if (st && st != reinterpret_cast<void*>(origGetDeviceState)
               && st != reinterpret_cast<void*>(origGetDeviceStateB) && !origGetDeviceStateB) {
        origGetDeviceStateB = reinterpret_cast<GetDeviceStateFn>(st);
        ok = attachOnce(reinterpret_cast<void**>(&origGetDeviceStateB),
                        reinterpret_cast<void*>(getDeviceStateHookedB))
             && ok;
        if (ok) {
            stateHooked.store(true);
        }
    }
    if (dd && !origGetDeviceData) {
        origGetDeviceData = reinterpret_cast<GetDeviceDataFn>(dd);
        attachOnce(reinterpret_cast<void**>(&origGetDeviceData),
                   reinterpret_cast<void*>(getDeviceDataHooked));
    }
    spdlog::info("dinput device methods state={:p}/{:p} data={:p} hooked={}",
                 reinterpret_cast<void*>(origGetDeviceState),
                 reinterpret_cast<void*>(origGetDeviceStateB),
                 reinterpret_cast<void*>(origGetDeviceData), stateHooked.load());
}

long __stdcall createDeviceHooked(void* thisptr, const Guid* rguid, void** out, void* outer)
{
    long hr = origCreateDevice ? origCreateDevice(thisptr, rguid, out, outer)
                               : static_cast<long>(0x80004005);
    spdlog::info("dinput CreateDevice hr={:x} dev={:p}", static_cast<unsigned>(hr),
                 (out && *out) ? *out : nullptr);
    if (hr >= 0 && out && *out) {
        hookDeviceMethods(*out);
    }
    return hr;
}

void hookCreateDeviceOf(void* di)
{
    if (!di) {
        return;
    }
    void** vtbl = *reinterpret_cast<void***>(di);
    if (!vtbl || !vtbl[kCreateDeviceVtbl]) {
        return;
    }
    if (createFnAttached.load()) {
        return;
    }
    origCreateDevice = reinterpret_cast<CreateDeviceFn>(vtbl[kCreateDeviceVtbl]);
    if (createFnAttached.exchange(true)) {
        return;
    }
    if (!attachOnce(reinterpret_cast<void**>(&origCreateDevice),
                    reinterpret_cast<void*>(createDeviceHooked))) {
        createFnAttached.store(false);
        spdlog::error("dinput CreateDevice hook failed");
        return;
    }
    spdlog::info("dinput CreateDevice hooked {:p}", reinterpret_cast<void*>(origCreateDevice));
}

long __stdcall directInputCreateAHooked(void* hinst, unsigned ver, void** ppDI, void* outer)
{
    long hr = origDirectInputCreateA ? origDirectInputCreateA(hinst, ver, ppDI, outer)
                                     : static_cast<long>(0x80004005);
    spdlog::info("DirectInputCreateA hr={:x} di={:p}", static_cast<unsigned>(hr),
                 (ppDI && *ppDI) ? *ppDI : nullptr);
    if (hr >= 0 && ppDI && *ppDI) {
        hookCreateDeviceOf(*ppDI);
    }
    return hr;
}

long __stdcall directInputCreateExHooked(void* hinst,
                                         unsigned ver,
                                         const Guid* iid,
                                         void** ppv,
                                         void* outer)
{
    long hr = origDirectInputCreateEx ? origDirectInputCreateEx(hinst, ver, iid, ppv, outer)
                                      : static_cast<long>(0x80004005);
    spdlog::info("DirectInputCreateEx hr={:x} di={:p}", static_cast<unsigned>(hr),
                 (ppv && *ppv) ? *ppv : nullptr);
    if (hr >= 0 && ppv && *ppv) {
        hookCreateDeviceOf(*ppv);
    }
    return hr;
}

long __stdcall directInput8CreateHooked(void* hinst,
                                        unsigned ver,
                                        const Guid* iid,
                                        void** ppv,
                                        void* outer)
{
    long hr = origDirectInput8Create ? origDirectInput8Create(hinst, ver, iid, ppv, outer)
                                     : static_cast<long>(0x80004005);
    spdlog::info("DirectInput8Create hr={:x} di={:p}", static_cast<unsigned>(hr),
                 (ppv && *ppv) ? *ppv : nullptr);
    if (hr >= 0 && ppv && *ppv) {
        hookCreateDeviceOf(*ppv);
    }
    return hr;
}

void tryHookCreateExports()
{
    if (createExportHooked.load()) {
        return;
    }
    HMODULE di8 = GetModuleHandleA("dinput8.dll");
    if (!di8) {
        di8 = LoadLibraryA("dinput8.dll");
    }
    HMODULE di7 = GetModuleHandleA("dinput.dll");
    if (!di7) {
        di7 = LoadLibraryA("dinput.dll");
    }
    if (!di8 && !di7) {
        spdlog::error("dinput modules missing");
        return;
    }
    if (di8) {
        origDirectInput8Create = reinterpret_cast<DirectInput8CreateFn>(
            GetProcAddress(di8, "DirectInput8Create"));
    }
    if (di7) {
        origDirectInputCreateA = reinterpret_cast<DirectInputCreateAFn>(
            GetProcAddress(di7, "DirectInputCreateA"));
        origDirectInputCreateEx = reinterpret_cast<DirectInputCreateExFn>(
            GetProcAddress(di7, "DirectInputCreateEx"));
    }
    if (!origDirectInput8Create && !origDirectInputCreateA && !origDirectInputCreateEx) {
        spdlog::error("dinput create exports missing");
        return;
    }
    if (createExportHooked.exchange(true)) {
        return;
    }
    bool ok = true;
    if (origDirectInput8Create) {
        ok = attachOnce(reinterpret_cast<void**>(&origDirectInput8Create),
                        reinterpret_cast<void*>(directInput8CreateHooked))
             && ok;
    }
    if (origDirectInputCreateA) {
        ok = attachOnce(reinterpret_cast<void**>(&origDirectInputCreateA),
                        reinterpret_cast<void*>(directInputCreateAHooked))
             && ok;
    }
    if (origDirectInputCreateEx) {
        ok = attachOnce(reinterpret_cast<void**>(&origDirectInputCreateEx),
                        reinterpret_cast<void*>(directInputCreateExHooked))
             && ok;
    }
    spdlog::info("dinput create exports hooked ok={} di8={:p} createA={:p} createEx={:p}", ok,
                 reinterpret_cast<void*>(origDirectInput8Create),
                 reinterpret_cast<void*>(origDirectInputCreateA),
                 reinterpret_cast<void*>(origDirectInputCreateEx));
    if (!ok) {
        createExportHooked.store(false);
    }
}

void tryDummyMouse()
{
    if (stateHooked.load()) {
        return;
    }
    void* hinst = GetModuleHandleA(nullptr);
    if (origDirectInput8Create) {
        void* di = nullptr;
        const long hr = origDirectInput8Create(hinst, kDiVersion8, &iidDirectInput8A(), &di, nullptr);
        spdlog::info("dummy DI8 hr={:x} di={:p}", static_cast<unsigned>(hr), di);
        if (hr >= 0 && di) {
            hookCreateDeviceOf(di);
            void* dev = nullptr;
            const long dhr = (origCreateDevice ? origCreateDevice : createDeviceHooked)(
                di, &guidSysMouse(), &dev, nullptr);
            spdlog::info("dummy DI8 CreateDevice hr={:x} dev={:p}", static_cast<unsigned>(dhr),
                         dev);
            if (dev) {
                hookDeviceMethods(dev);
                reinterpret_cast<ReleaseFn>((*reinterpret_cast<void***>(dev))[2])(dev);
            }
            reinterpret_cast<ReleaseFn>((*reinterpret_cast<void***>(di))[2])(di);
        }
    }
    void* di = nullptr;
    long hr = -1;
    if (origDirectInputCreateEx) {
        hr = origDirectInputCreateEx(hinst, kDiVersion7, &iidDirectInput7A(), &di, nullptr);
        spdlog::info("dummy CreateEx hr={:x} di={:p}", static_cast<unsigned>(hr), di);
    }
    if ((hr < 0 || !di) && origDirectInputCreateA) {
        hr = origDirectInputCreateA(hinst, kDiVersion7, &di, nullptr);
        spdlog::info("dummy CreateA hr={:x} di={:p}", static_cast<unsigned>(hr), di);
    }
    if (hr < 0 || !di) {
        return;
    }
    hookCreateDeviceOf(di);
    void* dev = nullptr;
    const long dhr = (origCreateDevice ? origCreateDevice : createDeviceHooked)(di, &guidSysMouse(),
                                                                               &dev, nullptr);
    spdlog::info("dummy DI7 CreateDevice hr={:x} dev={:p}", static_cast<unsigned>(dhr), dev);
    if (dev) {
        hookDeviceMethods(dev);
        reinterpret_cast<ReleaseFn>((*reinterpret_cast<void***>(dev))[2])(dev);
    }
    reinterpret_cast<ReleaseFn>((*reinterpret_cast<void***>(di))[2])(di);
}

short __stdcall getAsyncKeyStateHooked(int vKey)
{
    short r = origGetAsyncKeyState ? origGetAsyncKeyState(vKey) : 0;
    if (vKey == 0x01 && pulseActive()) {
        r = static_cast<short>(r | static_cast<short>(0x8001));
    }
    return r;
}

short __stdcall getKeyStateHooked(int nVirtKey)
{
    short r = origGetKeyState ? origGetKeyState(nVirtKey) : 0;
    if (nVirtKey == 0x01 && pulseActive()) {
        r = static_cast<short>(r | static_cast<short>(0x8001));
    }
    return r;
}

int __stdcall getKeyboardStateHooked(unsigned char* keyState)
{
    int r = origGetKeyboardState ? origGetKeyboardState(keyState) : 0;
    if (r && keyState && pulseActive()) {
        keyState[0x01] = static_cast<unsigned char>(keyState[0x01] | 0x80);
    }
    return r;
}

} // namespace

std::atomic<unsigned long> gPulseUntil{0};
std::atomic<int> gPulseX{0};
std::atomic<int> gPulseY{0};
std::atomic<int> gPulseStay{0};

bool mousePulseActive()
{
    const unsigned long now = GetTickCount();
    if (GetFileAttributesA(game::DinputDeviceApi::pulsePath()) != INVALID_FILE_ATTRIBUTES) {
        int px = 0;
        int py = 0;
        char tag[8]{};
        HANDLE hf = CreateFileA(game::DinputDeviceApi::pulsePath(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf != INVALID_HANDLE_VALUE) {
            char buf[32]{};
            unsigned long rd = 0;
            if (ReadFile(hf, buf, 31, &rd, nullptr) && rd > 0) {
                sscanf(buf, "%d,%d,%7s", &px, &py, tag);
            }
            CloseHandle(hf);
        }
        DeleteFileA(game::DinputDeviceApi::pulsePath());
        gPulseX.store(px);
        gPulseY.store(py);
        gPulseStay.store(tag[0] == 's' ? 1 : 0);
        gPulseUntil.store(now + game::DinputDeviceApi::kPulseMs);
        resetInjectPhase();
        spdlog::info("dinput LBUTTON pulse {}ms {},{} stay={}", game::DinputDeviceApi::kPulseMs, px,
                     py, gPulseStay.load());
    }
    return now < gPulseUntil.load();
}

bool mousePulseHeld()
{
    return GetTickCount() < gPulseUntil.load();
}

void mousePulseClient(int* x, int* y)
{
    if (x) {
        *x = gPulseX.load();
    }
    if (y) {
        *y = gPulseY.load();
    }
}

bool pulseWantsStay()
{
    return gPulseStay.load() != 0;
}

bool driveAutomationEnabled()
{
    return GetFileAttributesA("C:\\d2-drive.flag") != INVALID_FILE_ATTRIBUTES;
}

bool installUser32MouseHooks()
{
    if (!driveAutomationEnabled()) {
        return false;
    }
    if (user32Installed.exchange(true)) {
        return origGetAsyncKeyState != nullptr;
    }
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32) {
        user32Installed.store(false);
        return false;
    }
    origGetAsyncKeyState = reinterpret_cast<GetAsyncKeyStateFn>(
        GetProcAddress(user32, "GetAsyncKeyState"));
    origGetKeyState = reinterpret_cast<GetKeyStateFn>(GetProcAddress(user32, "GetKeyState"));
    origGetKeyboardState = reinterpret_cast<GetKeyboardStateFn>(
        GetProcAddress(user32, "GetKeyboardState"));
    if (!origGetAsyncKeyState) {
        user32Installed.store(false);
        return false;
    }
    bool ok = attachOnce(reinterpret_cast<void**>(&origGetAsyncKeyState),
                         reinterpret_cast<void*>(getAsyncKeyStateHooked));
    if (origGetKeyState) {
        attachOnce(reinterpret_cast<void**>(&origGetKeyState),
                   reinterpret_cast<void*>(getKeyStateHooked));
    }
    if (origGetKeyboardState) {
        attachOnce(reinterpret_cast<void**>(&origGetKeyboardState),
                   reinterpret_cast<void*>(getKeyboardStateHooked));
    }
    spdlog::info("user32 mouse hooks ok={} kbdState={:p}", ok,
                 reinterpret_cast<void*>(origGetKeyboardState));
    installUser32MessageHooks();
    if (!ok) {
        user32Installed.store(false);
    }
    return ok;
}

bool installDinputDeviceHooks()
{
    if (!driveAutomationEnabled()) {
        return false;
    }
    tryHookCreateExports();
    tryDummyMouse();
    return stateHooked.load() || createExportHooked.load();
}

static DWORD __stdcall dinputHookThread(LPVOID)
{
    for (int i = 0; i < 45; ++i) {
        installDinputDeviceHooks();
        if (stateHooked.load()) {
            spdlog::info("dinput GetDeviceState ready after {}", i);
            return 0;
        }
        Sleep(1000);
    }
    spdlog::error("dinput GetDeviceState not hooked after retries");
    return 0;
}

void startDinputMouseHookThread()
{
    if (!driveAutomationEnabled()) {
        return;
    }
    HANDLE th = CreateThread(nullptr, 0, dinputHookThread, nullptr, 0, nullptr);
    if (th) {
        CloseHandle(th);
        return;
    }
    spdlog::error("dinput hook thread create failed");
}

} // namespace hooks
