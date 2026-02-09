/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2025 Alexey Voskresensky.
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

#include "batlogichooks.h"
#include "batlogic.h"
#include "battlemsgdata.h"
#include "game.h"
#include "midunit.h"
#include "originalfunctions.h"
#include "unitinfolist.h"

#include "battlemsgdataviewmutable.h"
#include "gameutils.h"
#include "groupview.h"
#include "scripts.h"
#include <spdlog/spdlog.h>

namespace hooks {

void __fastcall battleTurnHooked(game::CBatLogic* thisptr,
                                 int /*%edx*/,
                                 game::CResultSender* resultSender,
                                 int groupBattleCondition,
                                 game::CMidgardID* a4,
                                 game::CMidgardID* a5)
{
    using namespace game;

    const auto& batLogicApi = CBatLogicApi::get();

    batLogicApi.updateUnitsBattleXp(thisptr->objectMap, thisptr->battleMsgData);

    getOriginalFunctions().battleTurn(thisptr, resultSender, groupBattleCondition, a4, a5);
}

void __fastcall updateGroupsIfBattleIsOverHooked(game::CBatLogic* thisptr,
                                                 int /*%edx*/,
                                                 game::CResultSender* resultSender)
{
    using namespace game;
    auto& batLogicApi = CBatLogicApi::get();

    if (!batLogicApi.isBattleOver(thisptr)) {
        return;
    }

    const auto& battleApi = BattleMsgDataApi::get();
    const auto& fn = game::gameFunctions();
    const auto& listApi = UnitInfoListApi::get();
    const auto& idApi = CMidgardIDApi::get();
    auto* msgData = thisptr->battleMsgData;
    auto* objMap = thisptr->objectMap;

    batLogicApi.checkAndDestroyEquippedBattleItems(objMap, msgData);

    UnitInfoList unitInfos{};
    listApi.constructor(&unitInfos);
    battleApi.getUnitInfos(msgData, &unitInfos, false);
    listApi.sortBy(&unitInfos, listApi.compareByUnsummonOrder);

    CMidgardID tempId;

    for (const auto& unitInfo : unitInfos) {
        idApi.validateId(&tempId, unitInfo.unitId1);

        if (CMidUnit* unit = fn.findUnitById(objMap, &tempId)) {
            if (battleApi.getUnitStatus(msgData, &tempId, BattleStatus::Summon)) {
                batLogicApi.applyCBatAttackUnsummonEffect(objMap, &tempId, msgData, resultSender);
            }
            else if (unit->transformed) {
                bool retreated = battleApi.getUnitStatus(msgData, &tempId, BattleStatus::Retreated);
                batLogicApi.applyCBatAttackUntransformEffect(objMap, &tempId, msgData, resultSender,
                                                             !retreated);
            }
        }
    }
    listApi.destructor(&unitInfos);

    CMidgardID winnerGroupId;
    batLogicApi.getBattleWinnerGroupId(thisptr, &winnerGroupId);

    batLogicApi.applyCBatAttackGroupUpgrade(objMap, &winnerGroupId, msgData, resultSender);
    batLogicApi.applyCBatAttackGroupBattleCount(objMap, &winnerGroupId, msgData, resultSender);
    batLogicApi.restoreLeaderPositionsAfterDuel(objMap, msgData);

    static const auto scriptPath = scriptsFolder() / "hooks/hooks.lua";
    std::optional<sol::environment> env;

    if (auto OnBattleEnd = getScriptFunction(scriptPath, "OnBattleEnd", env, false, true)) {
        try {
            auto* globalObjMap = hooks::getObjectMap();
            if (auto* winnerGroup = hooks::getGroup(globalObjMap, &winnerGroupId)) {
                const bindings::GroupView win{winnerGroup, globalObjMap, &winnerGroupId};
                (*OnBattleEnd)(win);
            }
        } catch (const std::exception& e) {
            showErrorMessageBox(fmt::format("Lua Error (OnBattleEnd): {:s}", e.what()));
        }
    }
}

} // namespace hooks
