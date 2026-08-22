/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Stanislav Egorov.
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

#include "phasegamehooks.h"
#include "cmdstackvisitmsg.h"
#include "commandmsg.h"
#include "dinputdeviceapi.h"
#include "dinputdevicehooks.h"
#include "endturnapi.h"
#include "gameutils.h"
#include "mainview2.h"
#include "mainview2hooks.h"
#include "midclient.h"
#include "midclientcore.h"
#include "midgard.h"
#include "midgardid.h"
#include "midobjectlock.h"
#include "mqpoint.h"
#include "originalfunctions.h"
#include "phase.h"
#include "phasegame.h"
#include "stackmovemsg.h"
#include "utils.h"
#include "midstack.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <spdlog/spdlog.h>

namespace hooks {

static bool g_movedThisTick = false;
static unsigned long g_moveAtMs = 0;
static std::atomic<int> g_leftoverRestoreId{0};
static std::atomic<int> g_leftoverRestoreMp{-1};
static std::atomic<int> g_leftoverHudTicks{0};
static constexpr int kLeftoverHudTicks = 8;
static game::EndTurnApi::Api::SendEndTurnMsg sendEndTurnMsgOrig;

void requestLeftoverRestore(const game::CMidgardID* stackId, int movement)
{
    if (!stackId || movement < 0) {
        return;
    }
    g_leftoverRestoreId.store(stackId->value);
    g_leftoverRestoreMp.store(movement);
    g_leftoverHudTicks.store(0);
}

void clearLeftoverRestore()
{
    g_leftoverRestoreMp.store(-1);
    g_leftoverRestoreId.store(0);
    g_leftoverHudTicks.store(0);
}

bool applyLeftoverRestoreToMap(game::IMidgardObjectMap* objectMap)
{
    using namespace game;
    const int mp = g_leftoverRestoreMp.load();
    if (mp < 0 || !objectMap) {
        return false;
    }
    const int idVal = g_leftoverRestoreId.load();
    CMidgardID stackId{};
    stackId.value = idVal;
    CMidStack* stack = getStack(objectMap, &stackId);
    if (!stack) {
        spdlog::info("[LEFTOVER] restore miss id={} want={}", idVal, mp);
        return false;
    }
    const int have = static_cast<int>(stack->movement);
    if (have < mp) {
        const int clamped = mp > 255 ? 255 : mp;
        stack->movement = static_cast<std::uint8_t>(clamped);
    }
    spdlog::info("[LEFTOVER] restore id={} have={} want={} now={}", idVal, have, mp,
                 static_cast<int>(stack->movement));
    return static_cast<int>(stack->movement) >= mp;
}

void applyLeftoverRestore(game::CPhaseGame* thisptr)
{
    using namespace game;
    if (g_leftoverRestoreMp.load() < 0) {
        return;
    }
    if (GetFileAttributesA(DinputDeviceApi::endTurnCmdPath()) != INVALID_FILE_ATTRIBUTES) {
        return;
    }
    if (thisptr && thisptr->data && !thisptr->data->clientTakesTurn) {
        return;
    }
    if (g_leftoverHudTicks.load() >= kLeftoverHudTicks) {
        return;
    }
    if (thisptr && thisptr->data && thisptr->data->midObjectLock) {
        thisptr->data->midObjectLock->patched.movingStack = false;
    }
    if (!thisptr) {
        return;
    }
    if (applyLeftoverRestoreToMap(CPhaseApi::get().getDataCache(&thisptr->phase))) {
        g_leftoverHudTicks.fetch_add(1);
    }
}

bool trySendStackMoveCmd(game::CPhaseGame* thisptr)
{
    using namespace game;
    applyLeftoverRestore(thisptr);
    if (!thisptr || !thisptr->data || !thisptr->data->clientTakesTurn) {
        return false;
    }
    if (thisptr->data->midObjectLock && thisptr->data->midObjectLock->patched.movingStack) {
        return false;
    }
    const char* path = DinputDeviceApi::stackMoveCmdPath();
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    char buf[80]{};
    HANDLE hf = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        return false;
    }
    unsigned long rd = 0;
    ReadFile(hf, buf, 79, &rd, nullptr);
    CloseHandle(hf);
    DeleteFileA(path);
    for (char* p = buf; *p; ++p) {
        if (*p == ',') {
            *p = ' ';
        }
    }
    char idStr[16]{};
    int sx = 0;
    int sy = 0;
    int ex = 0;
    int ey = 0;
    if (sscanf(buf, "%15s %d %d %d %d", idStr, &sx, &sy, &ex, &ey) != 5) {
        spdlog::info("stackmove cmd parse fail");
        return false;
    }
    CMidgardID stackId{};
    CMidgardIDApi::get().fromString(&stackId, idStr);
    CMqPoint start{sx, sy};
    CMqPoint end{ex, ey};
    CStackMoveMsg tmp;
    CStackMoveMsgApi::get().constructor(&tmp);
    appendStackMoveStep(&tmp.movementPath, &end, 1);
    CPhaseGameApi::get().sendStackMoveMsg(thisptr, &stackId, &tmp.movementPath, &start, &end);
    g_movedThisTick = true;
    g_moveAtMs = GetTickCount();
    spdlog::info("stackmove cmd {} ({},{}) -> ({},{}) pathlen={}", idStr, sx, sy, ex, ey,
                 tmp.movementPath.length);
    CStackMoveMsgApi::get().destructor(&tmp);
    return true;
}

static void trySendVisitSiteCmd(game::CPhaseGame* thisptr)
{
    using namespace game;
    if (!thisptr || !thisptr->data || !thisptr->data->clientTakesTurn) {
        return;
    }
    if (g_movedThisTick) {
        return;
    }
    if (g_moveAtMs && GetTickCount() - g_moveAtMs < 1500) {
        spdlog::info("visitsite skip debounce");
        return;
    }
    if (thisptr->data->midObjectLock && thisptr->data->midObjectLock->patched.movingStack) {
        spdlog::info("visitsite skip moving");
        return;
    }
    const char* path = DinputDeviceApi::visitSiteCmdPath();
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    char buf[48]{};
    HANDLE hf = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        return;
    }
    unsigned long rd = 0;
    ReadFile(hf, buf, 47, &rd, nullptr);
    CloseHandle(hf);
    DeleteFileA(path);
    for (char* p = buf; *p; ++p) {
        if (*p == ',') {
            *p = ' ';
        }
    }
    char stackStr[16]{};
    char siteStr[16]{};
    if (sscanf(buf, "%15s %15s", stackStr, siteStr) != 2) {
        spdlog::info("visitsite cmd parse fail");
        return;
    }
    CMainView2* view = rememberedMainView();
    if (!view) {
        spdlog::info("visitsite cmd no view");
        return;
    }
    CCommandMsg* base = getOriginalFunctions().commandMsgCreate(CommandMsgId::StackVisit);
    if (!base) {
        spdlog::info("visitsite cmd create fail");
        return;
    }
    auto* msg = static_cast<CCmdStackVisitMsg*>(base);
    CMidgardIDApi::get().fromString(&msg->visitorStackId, stackStr);
    CMidgardIDApi::get().fromString(&msg->siteId, siteStr);
    if (thisptr->data) {
        msg->playerId = thisptr->data->currentPlayerId;
    }
    getOriginalFunctions().handleCmdStackVisitMsg(view, base);
    spdlog::info("visitsite cmd {} {}", stackStr, siteStr);
}

void __fastcall sendEndTurnMsgHooked(game::CPhaseGame* thisptr, int /*%edx*/)
{
    using namespace game;
    clearLeftoverRestore();
    if (g_movedThisTick) {
        spdlog::info("endturn skip movedThisTick");
        return;
    }
    if (thisptr && thisptr->data && thisptr->data->midObjectLock) {
        thisptr->data->midObjectLock->patched.movingStack = false;
    }
    if (sendEndTurnMsgOrig) {
        sendEndTurnMsgOrig(thisptr);
    }
}

void* getSendEndTurnMsgHooked()
{
    return (void*)sendEndTurnMsgHooked;
}

void** getSendEndTurnMsgOrig()
{
    return (void**)&sendEndTurnMsgOrig;
}

bool trySendEndTurnCmd(game::CPhaseGame* thisptr)
{
    using namespace game;
    static bool sending = false;
    if (sending) {
        return false;
    }
    if (mousePulseHeld()) {
        return false;
    }
    if (g_movedThisTick) {
        spdlog::info("endturn cmd skip moved");
        return false;
    }
    const char* path = DinputDeviceApi::endTurnCmdPath();
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if (!thisptr || !thisptr->data) {
        DeleteFileA(path);
        spdlog::info("endturn cmd skip no phase");
        return true;
    }
    if (thisptr->data->midObjectLock && thisptr->data->midObjectLock->patched.movingStack) {
        spdlog::info("endturn cmd skip moving");
        return false;
    }
    auto send = EndTurnApi::get().sendEndTurnMsg;
    if (!send) {
        DeleteFileA(path);
        spdlog::info("endturn cmd no api");
        return true;
    }
    DeleteFileA(path);
    sending = true;
    send(thisptr);
    sending = false;
    spdlog::info("endturn cmd player={} sent=hud", idToString(&thisptr->data->currentPlayerId));
    return true;
}

void tryPollEndTurnFromMidgard()
{
    using namespace game;
    if (GetFileAttributesA(DinputDeviceApi::endTurnCmdPath()) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    CMidgard* midgard = CMidgardApi::get().instance();
    if (!midgard || !midgard->data || !midgard->data->client || !midgard->data->client->data) {
        return;
    }
    if (!midgard->data->client->data->scenarioStarted) {
        return;
    }
    CPhase* phase = midgard->data->client->data->phase;
    if (!phase) {
        return;
    }
    auto* phaseGame = reinterpret_cast<CPhaseGame*>(reinterpret_cast<char*>(phase)
                                                    - offsetof(CPhaseGame, phase));
    if (!phaseGame->data || !phaseGame->data->midClient) {
        return;
    }
    trySendEndTurnCmd(phaseGame);
}

bool __fastcall phaseGameCheckObjectLockHooked(game::CPhaseGame* thisptr, int /*%edx*/)
{
    if (!thisptr || !thisptr->data) {
        return false;
    }
    if (driveAutomationEnabled()) {
        trySendStackMoveCmd(thisptr);
        trySendVisitSiteCmd(thisptr);
        trySendEndTurnCmd(thisptr);
    }
    g_movedThisTick = false;
    const auto* lock = thisptr->data->midObjectLock;
    if (!lock) {
        return false;
    }
    if (lock->patched.exportingLeader) {
        spdlog::debug(__FUNCTION__ ": unlocked due to exportingLeader");
        return false;
    }

    if (lock->patched.movingStack) {
        return true;
    }

    return lock->pendingLocalUpdates || lock->pendingNetworkUpdates;
}

void __fastcall phaseGameSendStackMoveMsgHooked(
    game::CPhaseGame* thisptr,
    int /*%edx*/,
    const game::CMidgardID* stackId,
    const game::List<game::Pair<game::CMqPoint, int>>* movementPath,
    const game::CMqPoint* startPosition,
    const game::CMqPoint* endPosition)
{
    using namespace game;

    const auto& stackMoveMsgApi = CStackMoveMsgApi::get();

    auto* data = thisptr->data;
    if (!data->clientTakesTurn) {
        return;
    }

    ++data->midObjectLock->pendingNetworkUpdates;
    data->midObjectLock->patched.movingStack = true;
    spdlog::debug(
        __FUNCTION__ ": CMidObjectLock::movingStack set to true, pendingNetworkUpdates incremented to {:d}",
        data->midObjectLock->pendingNetworkUpdates);

    CStackMoveMsg message;
    stackMoveMsgApi.constructor2(&message, stackId, movementPath, startPosition, endPosition);

    CMidClient* client = data->midClient;
    CMidgard* midgard = client->core.data->midgard;
    CMidgardApi::get().sendNetMsgToServer(midgard, &message);

    stackMoveMsgApi.destructor(&message);
}

} // namespace hooks
