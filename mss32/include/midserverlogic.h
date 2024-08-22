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

#ifndef MIDSERVERLOGIC_H
#define MIDSERVERLOGIC_H

#include "d2set.h"
#include "idlist.h"
#include "idset.h"
#include "midmsgsender.h"
#include "midserverlogiccore.h"

namespace game {

struct CMidServerLogic;
struct CMidServer;
struct CStreamBits;
struct AiLogic;
struct CMidgardScenarioMap;
struct IEventEffect;
struct CMqPoint;

/*
 * All the fields initially point to the same parent logic. Is this some kind of enumerable
 * collection pattern?
 */
struct CMidServerLogicData
{
    CMidServerLogic* logic;
    CMidServerLogic** logic2;
    CMidServerLogic** logic3;
};

assert_size(CMidServerLogicData, 12);

struct CMidServerLogicData2
{
    CMidgardID unknownId;
    int unknown;
    int unknown2;
    Vector<void*> array;
    Vector<void*> array2;
    Vector<void*> array3;
    Vector<void*> array4;
    Vector<void*> array5;
    Vector<void*> array6;
    Set<CMidgardID> list;
    Set<void> list2;
};

assert_size(CMidServerLogicData2, 164);
assert_offset(CMidServerLogicData2, array, 12);
assert_offset(CMidServerLogicData2, array2, 28);
assert_offset(CMidServerLogicData2, array3, 44);
assert_offset(CMidServerLogicData2, array4, 60);
assert_offset(CMidServerLogicData2, array5, 76);
assert_offset(CMidServerLogicData2, array6, 92);
assert_offset(CMidServerLogicData2, list, 108);
assert_offset(CMidServerLogicData2, list2, 136);

struct CMidServerLogic
    : public CMidServerLogicCore
    , public IMidMsgSender
{
    CMidServer* midServer;
    int unknown2;
    int unknown3;
    CMidServerLogicData data;
    Vector<AiLogic*> aiLogic;
    AiLogic* currentAi;
    List<CMidgardID> playersIdList;
    CStreamBits* streamBits;
    int unknown7;
    CMidServerLogicData2 data2;
    int currentPlayerIndex;
    int unknown9;
    bool turnNumberIsZero;
    char padding[3];
    // Assumption: List<Pair<CommandMsgId, CStreamBits>> messagesAndStreams;
    List<void*> list;
    int unknown11;
    int unknown12;
    std::uint32_t aiMessageId;
    int unknown14;
    bool upgradeLeaders;
    char padding2[3];
    int unknown16;
    CMidgardID winnerPlayerId;
    List<void*> list2;
    char unknown17;
    char padding3[3];
    BattleMsgData* battleMsgData;
};

assert_size(CMidServerLogic, 324);
assert_offset(CMidServerLogic, CMidServerLogic::IMidMsgSender::vftable, 8);
assert_offset(CMidServerLogic, data, 24);
assert_offset(CMidServerLogic, aiLogic, 36);
assert_offset(CMidServerLogic, playersIdList, 56);
assert_offset(CMidServerLogic, data2, 80);
assert_offset(CMidServerLogic, currentPlayerIndex, 244);
assert_offset(CMidServerLogic, unknown11, 272);
assert_offset(CMidServerLogic, list2, 300);
assert_offset(CMidServerLogic, unknown17, 316);

static inline CMidServerLogic* castMidMsgSenderToMidServerLogic(const IMidMsgSender* sender)
{
    return reinterpret_cast<CMidServerLogic*>(
        (uintptr_t)sender - offsetof(CMidServerLogic, CMidServerLogic::IMidMsgSender::vftable));
}

namespace CMidServerLogicApi {

struct Api
{
    using GetObjectMap = CMidgardScenarioMap*(__thiscall*)(const CMidServerLogic* thisptr);
    GetObjectMap getObjectMap;

    using SendRefreshInfo = bool(__thiscall*)(const CMidServerLogic* thisptr,
                                              const Set<CMidgardID>* objectsList,
                                              std::uint32_t playerNetId);
    SendRefreshInfo sendRefreshInfo;

    using StackExchangeItem = void(__thiscall*)(CMidServerLogicData* thisptr,
                                                const CMidgardID* playerId,
                                                const CMidgardID* fromStackId,
                                                const CMidgardID* toStackId,
                                                const IdSet* itemIds);
    StackExchangeItem stackExchangeItem;

    using ApplyEventEffectsAndCheckMidEventTriggerers =
        bool(__thiscall*)(CMidServerLogic** thisptr,
                          List<IEventEffect*>* effectsList,
                          const CMidgardID* triggererId,
                          const CMidgardID* playingStackId);
    ApplyEventEffectsAndCheckMidEventTriggerers applyEventEffectsAndCheckMidEventTriggerers;

    using StackMove = bool(__thiscall*)(CMidServerLogic** thisptr,
                                        const CMidgardID* playerId,
                                        List<Pair<CMqPoint, int>>* movementPath,
                                        const CMidgardID* stackId,
                                        const CMqPoint* startingPoint,
                                        const CMqPoint* endPoint);
    StackMove stackMove;

    using FilterAndProcessEventsNoPlayer = bool(__stdcall*)(IMidgardObjectMap* objectMap,
                                                            List<CMidEvent*>* eventObjectList,
                                                            List<IEventEffect*>* effectsList,
                                                            bool* stopProcessing,
                                                            IdList* executedEvents,
                                                            const CMidgardID* triggererStackId,
                                                            const CMidgardID* playingStackId);
    FilterAndProcessEventsNoPlayer filterAndProcessEventsNoPlayer;

    using CheckAndExecuteEvent = bool(__stdcall*)(IMidgardObjectMap* objectMap,
                                                  List<IEventEffect*>* effectsList,
                                                  bool* stopProcessing,
                                                  const CMidgardID* eventId,
                                                  const CMidgardID* playerId,
                                                  const CMidgardID* stackTriggererId,
                                                  const CMidgardID* playingStackId,
                                                  int samePlayer);
    CheckAndExecuteEvent checkAndExecuteEvent;

    using ExecuteEventEffects = void(__stdcall*)(IMidgardObjectMap* objectMap,
                                                 List<IEventEffect*>* effectsList,
                                                 bool* stopProcessing,
                                                 const CMidgardID* eventId,
                                                 const CMidgardID* playerId,
                                                 const CMidgardID* stackTriggererId,
                                                 const CMidgardID* playingStackId);
    ExecuteEventEffects executeEventEffects;

    using FilterAndProcessEvents = bool(__stdcall*)(IMidgardObjectMap* objectMap,
                                                    List<CMidEvent*>* eventObjectList,
                                                    List<IEventEffect*>* effectsList,
                                                    bool* stopProcessing,
                                                    IdList* executedEvents,
                                                    const CMidgardID* playerId,
                                                    const CMidgardID* triggererStackId,
                                                    const CMidgardID* playingStackId);
    FilterAndProcessEvents filterAndProcessEvents;

    using CheckEventConditions = bool(__stdcall*)(const IMidgardObjectMap* objectMap,
                                                  List<IEventEffect*>* effectsList,
                                                  const CMidgardID* playerId,
                                                  const CMidgardID* stackTriggererId,
                                                  int samePlayer,
                                                  const CMidgardID* eventId);
    CheckEventConditions checkEventConditions;

    using Constructor = CMidServerLogic*(__thiscall*)(CMidServerLogic* thisptr,
                                                      CMidServer* server,
                                                      bool multiplayerGame,
                                                      bool hotseatGame,
                                                      int a5,
                                                      int gameVersion);
    Constructor constructor;

    using GetPlayerInfo = NetPlayerInfo*(__thiscall*)(CMidServerLogic* thisptr,
                                                      std::uint32_t playerNetId);
    GetPlayerInfo getPlayerInfo;

    /** Returns true if player with specified id is current: it actively plays its turn. */
    using IsCurrentPlayer = bool(__stdcall*)(CMidServerLogic* serverLogic,
                                             const CMidgardID* playerId);
    IsCurrentPlayer isCurrentPlayer;
};

Api& get();

struct Vftable
{
    const void* mqNetTraffic;
    const IMidMsgSenderVftable* midMsgSender;
};

Vftable& vftable();

} // namespace CMidServerLogicApi

} // namespace game

#endif // MIDSERVERLOGIC_H
