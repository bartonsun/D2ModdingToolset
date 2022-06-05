/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Stanislav Egorov.
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

#include "gameutils.h"
#include "battlemsgdata.h"
#include "dynamiccast.h"
#include "editorglobaldata.h"
#include "fortification.h"
#include "game.h"
#include "midclient.h"
#include "midclientcore.h"
#include "midgard.h"
#include "midgardobjectmap.h"
#include "midgardplan.h"
#include "midgardscenariomap.h"
#include "midplayer.h"
#include "midruin.h"
#include "midscenvariables.h"
#include "midserver.h"
#include "midserverlogic.h"
#include "midstack.h"
#include "scenarioinfo.h"
#include "version.h"
#include <thread>

extern std::thread::id mainThreadId;

namespace hooks {

static game::CMidgardID createIdWithType(const game::IMidgardObjectMap* objectMap,
                                         game::IdType idType)
{
    using namespace game;

    const auto& idApi{CMidgardIDApi::get()};
    auto scenarioId{objectMap->vftable->getId(objectMap)};

    CMidgardID id{};
    idApi.fromParts(&id, idApi.getCategory(scenarioId), idApi.getCategoryIndex(scenarioId), idType,
                    0);

    return id;
}

bool isGreaterPickRandomIfEqual(int first, int second)
{
    using namespace game;

    const auto& fn = gameFunctions();

    return first > second || (first == second && fn.generateRandomNumber(2) == 1);
}

const game::IMidgardObjectMap* getObjectMap()
{
    using namespace game;

    if (gameVersion() == GameVersion::ScenarioEditor) {
        auto global = *EditorGlobalDataApi::get().getGlobalData();
        if (!global->initialized)
            return nullptr;

        return global->unknown2->data->scenarioMap;
    }

    auto midgard = CMidgardApi::get().instance();
    if (std::this_thread::get_id() == mainThreadId) {
        auto client = midgard->data->client;
        if (!client)
            return nullptr;

        return CMidClientCoreApi::get().getObjectMap(&client->core);
    } else {
        auto server = midgard->data->server;
        if (!server)
            return nullptr;

        return CMidServerLogicApi::get().getObjectMap(server->data->serverLogic);
    }
}

const game::CMidUnitGroup* getGroup(const game::IMidgardObjectMap* objectMap,
                                    const game::CMidgardID* groupId)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& rtti = RttiApi::rtti();
    const auto dynamicCast = RttiApi::get().dynamicCast;

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, groupId);
    switch (CMidgardIDApi::get().getType(groupId)) {
    case IdType::Stack: {
        auto stack = (const CMidStack*)dynamicCast(obj, 0, rtti.IMidScenarioObjectType,
                                                   rtti.CMidStackType, 0);
        return stack ? &stack->group : nullptr;
    }
    case IdType::Fortification: {
        auto fortification = static_cast<const CFortification*>(obj);
        return fortification ? &fortification->group : nullptr;
    }
    case IdType::Ruin:
        auto ruin = static_cast<const CMidRuin*>(obj);
        return ruin ? &ruin->group : nullptr;
    }

    return nullptr;
}

const game::CMidUnitGroup* getAllyOrEnemyGroup(const game::IMidgardObjectMap* objectMap,
                                               const game::BattleMsgData* battleMsgData,
                                               const game::CMidgardID* unitId,
                                               bool ally)
{
    using namespace game;

    const auto& fn = gameFunctions();

    CMidgardID groupId{};
    fn.getAllyOrEnemyGroupId(&groupId, battleMsgData, unitId, ally);

    void* tmp{};
    return fn.getStackFortRuinGroup(tmp, objectMap, &groupId);
}

const game::CScenarioInfo* getScenarioInfo(const game::IMidgardObjectMap* objectMap)
{
    const auto id{createIdWithType(objectMap, game::IdType::ScenarioInfo)};

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id);
    if (!obj) {
        return nullptr;
    }

    return static_cast<const game::CScenarioInfo*>(obj);
}

const game::CMidPlayer* getPlayer(const game::IMidgardObjectMap* objectMap,
                                  const game::CMidgardID* playerId)
{
    using namespace game;

    auto playerObj = objectMap->vftable->findScenarioObjectById(objectMap, playerId);
    const auto& rtti = RttiApi::rtti();
    const auto dynamicCast = RttiApi::get().dynamicCast;
    return (const CMidPlayer*)dynamicCast(playerObj, 0, rtti.IMidScenarioObjectType,
                                          rtti.CMidPlayerType, 0);
}

const game::CMidPlayer* getPlayer(const game::IMidgardObjectMap* objectMap,
                                  const game::BattleMsgData* battleMsgData,
                                  const game::CMidgardID* unitId)
{
    using namespace game;

    const auto& battle = BattleMsgDataApi::get();
    CMidgardID playerId = battle.isUnitAttacker(battleMsgData, unitId)
                              ? battleMsgData->attackerPlayerId
                              : battleMsgData->defenderPlayerId;

    return getPlayer(objectMap, &playerId);
}

const game::CMidScenVariables* getScenarioVariables(const game::IMidgardObjectMap* objectMap)
{
    const auto id{createIdWithType(objectMap, game::IdType::ScenarioVariable)};

    auto obj{objectMap->vftable->findScenarioObjectById(objectMap, &id)};
    if (!obj) {
        return nullptr;
    }

    return static_cast<const game::CMidScenVariables*>(obj);
}

const game::CMidgardPlan* getMidgardPlan(const game::IMidgardObjectMap* objectMap)
{
    const auto id{createIdWithType(objectMap, game::IdType::Plan)};

    auto obj{objectMap->vftable->findScenarioObjectById(objectMap, &id)};
    if (!obj) {
        return nullptr;
    }

    return static_cast<const game::CMidgardPlan*>(obj);
}

game::CMidStack* getStack(const game::IMidgardObjectMap* objectMap, const game::CMidgardID* stackId)
{
    using namespace game;

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, stackId);
    if (!obj) {
        return nullptr;
    }

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    return (CMidStack*)dynamicCast(obj, 0, rtti.IMidScenarioObjectType, rtti.CMidStackType, 0);
}

game::CMidStack* getStack(const game::IMidgardObjectMap* objectMap,
                          const game::BattleMsgData* battleMsgData,
                          const game::CMidgardID* unitId)
{
    using namespace game;

    const auto& battle = BattleMsgDataApi::get();
    CMidgardID groupId = battle.isUnitAttacker(battleMsgData, unitId)
                             ? battleMsgData->attackerGroupId
                             : battleMsgData->defenderGroupId;

    return getStack(objectMap, &groupId);
}

const game::CMidStack* getStackByUnitId(const game::IMidgardObjectMap* objectMap,
                                        const game::CMidgardID* unitId)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& rtti = RttiApi::rtti();
    const auto dynamicCast = RttiApi::get().dynamicCast;

    auto stackId = fn.getStackIdByUnitId(objectMap, unitId);
    if (!stackId)
        return nullptr;

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, stackId);
    return (const CMidStack*)dynamicCast(obj, 0, rtti.IMidScenarioObjectType, rtti.CMidStackType,
                                         0);
}

} // namespace hooks
