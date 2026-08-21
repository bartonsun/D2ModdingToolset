#include "dinputdevicehooks.h"
#include "gameutils.h"
#include "interface.h"
#include "interfmanager.h"
#include "midclient.h"
#include "midgard.h"
#include "midgardid.h"
#include "midstack.h"
#include "mqpoint.h"
#include "mquicontrollersimple.h"
#include "phase.h"
#include "phasegame.h"
#include "phasegamehooks.h"
#include "stackmovemsg.h"
#include "smartptr.h"
#include "uimanager.h"
#include <cstddef>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <detours.h>
#include <spdlog/spdlog.h>

#include <atomic>

namespace {

struct Msg
{
    void* hwnd;
    unsigned message;
    unsigned wParam;
    long lParam;
    unsigned time;
    long ptX;
    long ptY;
};

using PeekMessageAFn = int(__stdcall*)(Msg* msg, void* hwnd, unsigned min, unsigned max, unsigned remove);
using GetMessageAFn = int(__stdcall*)(Msg* msg, void* hwnd, unsigned min, unsigned max);

PeekMessageAFn origPeekMessageA = nullptr;
GetMessageAFn origGetMessageA = nullptr;
using RunUpdateFn = int(__thiscall*)(void* thisptr);
RunUpdateFn origRunUpdate = nullptr;
std::atomic<int> injectPhase{0};
std::atomic<bool> msgHooked{false};
std::atomic<bool> clickRequested{false};
std::atomic<bool> updateHooked{false};

bool rangeAllows(unsigned min, unsigned max, unsigned message)
{
    if (min == 0 && max == 0) {
        return true;
    }
    return message >= min && message <= max;
}

void fillClick(Msg* msg, void* hwnd, unsigned message)
{
    int px = 0;
    int py = 0;
    hooks::mousePulseClient(&px, &py);
    POINT client{px, py};
    void* gameHwnd = FindWindowA("MQ_UIManager", nullptr);
    if (!gameHwnd) {
        gameHwnd = FindWindowA(nullptr, "Disciples II");
    }
    void* target = gameHwnd ? gameHwnd : (hwnd ? hwnd : GetForegroundWindow());
    POINT screen = client;
    if (target) {
        ClientToScreen(static_cast<HWND>(target), &screen);
        SetCursorPos(screen.x, screen.y);
    }
    msg->hwnd = target;
    msg->message = message;
    msg->wParam = (message == 0x0201) ? 1u : 0u;
    msg->lParam = (static_cast<unsigned>(client.y) << 16) | (static_cast<unsigned>(client.x) & 0xFFFFu);
    msg->time = GetTickCount();
    msg->ptX = screen.x;
    msg->ptY = screen.y;
}

bool injectIfPulsed(Msg* msg, void* hwnd, unsigned min, unsigned max, bool consume)
{
    if (!msg || !hooks::mousePulseActive()) {
        return false;
    }
    int phase = injectPhase.load();
    if (phase >= 2) {
        return false;
    }
    unsigned message = (phase == 0) ? 0x0201u : 0x0202u;
    if (!rangeAllows(min, max, message)) {
        return false;
    }
    fillClick(msg, hwnd, message);
    if (consume) {
        injectPhase.store(phase == 0 ? 1 : 2);
    }
    if (phase == 0 && hooks::pulseWantsStay()) {
        hooks::requestGameMapClick();
    }
    spdlog::info("user32 inject WM {:x} hwnd={:p} lp={:x}", message, msg->hwnd,
                 static_cast<unsigned>(msg->lParam));
    return true;
}

int __stdcall peekMessageAHooked(Msg* msg, void* hwnd, unsigned min, unsigned max, unsigned remove)
{
    if (injectIfPulsed(msg, hwnd, min, max, (remove & 1u) != 0)) {
        return 1;
    }
    hooks::tryPollEndTurnFromMidgard();
    int r = origPeekMessageA ? origPeekMessageA(msg, hwnd, min, max, remove) : 0;
    if (clickRequested.exchange(false)) {
        hooks::fireGameMapClick();
    }
    return r;
}

int __stdcall getMessageAHooked(Msg* msg, void* hwnd, unsigned min, unsigned max)
{
    if (injectIfPulsed(msg, hwnd, min, max, true)) {
        return 1;
    }
    return origGetMessageA ? origGetMessageA(msg, hwnd, min, max) : 0;
}

} // namespace

int __fastcall runUpdateHooked(void* thisptr, int)
{
    if (clickRequested.exchange(false)) {
        hooks::fireGameMapClick();
    }
    hooks::tryPollEndTurnFromMidgard();
    return origRunUpdate ? origRunUpdate(thisptr) : 1;
}

void hookRunUpdate(void* controller)
{
    if (!controller || updateHooked.load()) {
        return;
    }
    void** vtbl = *reinterpret_cast<void***>(controller);
    if (!vtbl || !vtbl[19]) {
        return;
    }
    origRunUpdate = reinterpret_cast<RunUpdateFn>(vtbl[19]);
    if (updateHooked.exchange(true)) {
        return;
    }
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    const long att = DetourAttach(reinterpret_cast<void**>(&origRunUpdate),
                                  reinterpret_cast<void*>(runUpdateHooked));
    const long hr = DetourTransactionCommit();
    spdlog::info("runUpdate hook att={} hr={} fn={:p}", att, hr,
                 reinterpret_cast<void*>(origRunUpdate));
    if (att != 0 || hr != 0) {
        updateHooked.store(false);
    }
}

namespace hooks {

void requestGameMapClick()
{
    clickRequested.store(true);
    using namespace game;
    UIManagerPtr manager;
    CUIManagerApi::get().get(&manager);
    if (manager.data && manager.data->data && manager.data->data->uiController) {
        hookRunUpdate(manager.data->data->uiController);
    }
    SmartPointerApi::get().createOrFree(reinterpret_cast<SmartPointer*>(&manager), nullptr);
}

void fireGameMapClick()
{
    using namespace game;
    int px = 0;
    int py = 0;
    mousePulseClient(&px, &py);
    CMidgard* midgard = CMidgardApi::get().instance();
    if (!midgard || !midgard->data || !midgard->data->client || !midgard->data->client->data) {
        spdlog::info("stayMove no midgard {},{}", px, py);
        return;
    }
    CPhase* phase = midgard->data->client->data->phase;
    if (!phase) {
        spdlog::info("stayMove no phase");
        return;
    }
    auto* phaseGame = reinterpret_cast<CPhaseGame*>(reinterpret_cast<char*>(phase)
                                                    - offsetof(CPhaseGame, phase));
    if (!phaseGame->data || !phaseGame->data->clientTakesTurn) {
        spdlog::info("stayMove no turn");
        return;
    }
    if (hooks::trySendStackMoveCmd(phaseGame)) {
        return;
    }
    CMidgardID stackId{};
    CMidgardIDApi::get().fromString(&stackId, "S143KC0000");
    auto* objectMap = CPhaseApi::get().getDataCache(phase);
    const CMidStack* stack = getStack(objectMap, &stackId);
    if (!stack) {
        spdlog::info("stayMove no magot");
        return;
    }
    CMqPoint start = stack->position;
    CMqPoint end = start;
    CStackMoveMsg tmp;
    CStackMoveMsgApi::get().constructor(&tmp);
    appendStackMoveStep(&tmp.movementPath, &end, 1);
    spdlog::info("stayMove magot ({},{}) -> ({},{}) mp={} pathlen={}", start.x, start.y, end.x,
                 end.y, static_cast<int>(stack->movement), tmp.movementPath.length);
    CPhaseGameApi::get().sendStackMoveMsg(phaseGame, &stackId, &tmp.movementPath, &start, &end);
    CStackMoveMsgApi::get().destructor(&tmp);
}

void resetInjectPhase()
{
    injectPhase.store(0);
}

void installUser32MessageHooks()
{
    if (!driveAutomationEnabled()) {
        return;
    }
    if (msgHooked.exchange(true)) {
        return;
    }
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32) {
        msgHooked.store(false);
        return;
    }
    origPeekMessageA = reinterpret_cast<PeekMessageAFn>(GetProcAddress(user32, "PeekMessageA"));
    origGetMessageA = reinterpret_cast<GetMessageAFn>(GetProcAddress(user32, "GetMessageA"));
    if (!origPeekMessageA) {
        msgHooked.store(false);
        return;
    }
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<void**>(&origPeekMessageA), reinterpret_cast<void*>(peekMessageAHooked));
    if (origGetMessageA) {
        DetourAttach(reinterpret_cast<void**>(&origGetMessageA), reinterpret_cast<void*>(getMessageAHooked));
    }
    const long hr = DetourTransactionCommit();
    spdlog::info("user32 Peek/GetMessage hook {}", hr);
    using namespace game;
    UIManagerPtr manager;
    CUIManagerApi::get().get(&manager);
    if (manager.data && manager.data->data && manager.data->data->uiController) {
        hookRunUpdate(manager.data->data->uiController);
    }
    SmartPointerApi::get().createOrFree(reinterpret_cast<SmartPointer*>(&manager), nullptr);
    if (hr != NO_ERROR) {
        msgHooked.store(false);
    }
}

} // namespace hooks
