/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Vladimir Makeev.
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

#ifndef MIDMSGSENDER_H
#define MIDMSGSENDER_H

#include "d2assert.h"

namespace game {

struct IMidMsgSenderVftable;
struct CMidgardID;
struct CCommandMsg;
struct CBatLogic;
struct BattleMsgData;

struct IMidMsgSender
{
    IMidMsgSenderVftable* vftable;
};

struct IMidMsgSenderVftable
{
    using Destructor = void(__thiscall*)(IMidMsgSender* thisptr, char flags);
    Destructor destructor;

    /** Sends objects changes using CRefreshInfo and CCmdEraseObj messages. */
    using SendObjectsChanges = bool(__thiscall*)(IMidMsgSender* thisptr);
    SendObjectsChanges sendObjectsChanges;

    /** Sets player id to the message and calls 'sendMessage'. */
    using SendPlayerMessage = bool(__thiscall*)(IMidMsgSender* thisptr,
                                                CCommandMsg* commandMsg,
                                                const CMidgardID* playerId,
                                                bool sendBeforeObjectsChanges);
    SendPlayerMessage sendPlayerMessage;

    /**
     * Sends command message.
     * CMidServerLogic checks for player info, message queue
     * and command message type before sending.
     * Calls sendObjectsChanges before or after sending the message (depends on
     * sendBeforeObjectsChanges parameter).
     */
    using SendMessage = bool(__thiscall*)(IMidMsgSender* thisptr,
                                          CCommandMsg* commandMsg,
                                          bool sendBeforeObjectsChanges);
    SendMessage sendMessage;

    /** Starts battle between two unit groups. */
    using StartBattle = int(__thiscall*)(IMidMsgSender* thisptr,
                                         bool duelMode,
                                         const CMidgardID* attackerGroupId,
                                         const CMidgardID* defenderGroupId);
    StartBattle startBattle;

    /** Ends battle and applies battle results. */
    using EndBattle = char(__thiscall*)(IMidMsgSender* thisptr,
                                        CBatLogic* batLogic,
                                        BattleMsgData* battleMsgData);
    EndBattle endBattle;

    /** Adds a single fast battle action to the AI message queue. */
    using FastBattleAction = bool(__thiscall*)(IMidMsgSender* thisptr,
                                               const BattleMsgData* battleMsgData,
                                               const CMidgardID* unitId);
    FastBattleAction fastBattleAction;

    /**
     * Meaning assumed.
     * Set to 1 before sending CCmdUpdateLeadersMsg in the method of CEffectPlayerTurn class.
     * Reset to 0 in another method of CEffectPlayerTurn.
     */
    using SetUpgradeLeaders = bool(__thiscall*)(IMidMsgSender* thisptr, bool value);
    SetUpgradeLeaders setUpgradeLeaders;

    /** Sets player with specified as scenario winner. In case of empty id all players lose. */
    using WinScenario = bool(__thiscall*)(IMidMsgSender* thisptr, const CMidgardID* winnerPlayerId);
    WinScenario winScenario;

    /**
     * Meaning assumed.
     * Sends CCmdExportLeaderMsg command.
     */
    using ExportLeaders = bool(__thiscall*)(IMidMsgSender* thisptr);
    ExportLeaders exportLeaders;
};

assert_vftable_size(IMidMsgSenderVftable, 10);

} // namespace game

#endif // MIDMSGSENDER_H
