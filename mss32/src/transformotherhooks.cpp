/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Stanislav Egorov.
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

#include "transformotherhooks.h"
#include "attack.h"
#include "batattacktransformother.h"
#include "battleattackinfo.h"
#include "battlemsgdata.h"
#include "battlemsgdataview.h"
#include "game.h"
#include "gameutils.h"
#include "globaldata.h"
#include "itemview.h"
#include "midgardobjectmap.h"
#include "midunit.h"
#include "scripts.h"
#include "settings.h"
#include "unitgenerator.h"
#include "unitimplview.h"
#include "unitutils.h"
#include "unitview.h"
#include "ussoldier.h"
#include "usunitimpl.h"
#include "utils.h"
#include "visitors.h"
#include <spdlog/spdlog.h>

namespace hooks {

static int getTransformOtherLevel(const game::CMidUnit* unit,
                                  const game::CMidUnit* targetUnit,
                                  game::TUsUnitImpl* transformImpl,
                                  const game::IMidgardObjectMap* objectMap,
                                  const game::CMidgardID* unitOrItemId,
                                  const game::BattleMsgData* battleMsgData)
{
    using namespace game;

    // The function is only accessed by the server thread - the single instance is enough.
    static std::optional<sol::environment> env;
    static std::optional<sol::function> getLevel;
    const auto path{scriptsFolder() / "transformOther.lua"};
    if (!env && !getLevel) {
        getLevel = getScriptFunction(path, "getLevel", env, true, true);
    }
    if (!getLevel) {
        return 0;
    }

    try {
        const bindings::UnitView attacker{unit};
        const bindings::UnitView target{targetUnit};
        const bindings::UnitImplView impl{transformImpl};
        const bindings::BattleMsgDataView battleView{battleMsgData, objectMap};

        if (CMidgardIDApi::get().getType(unitOrItemId) == IdType::Item) {
            const bindings::ItemView itemView{unitOrItemId, objectMap};
            return (*getLevel)(attacker, target, impl, &itemView, battleView);
        } else
            return (*getLevel)(attacker, target, impl, nullptr, battleView);
    } catch (const std::exception& e) {
        showErrorMessageBox(fmt::format("Failed to run '{:s}' script.\n"
                                        "Reason: '{:s}'",
                                        path.string(), e.what()));
        return 0;
    }
}

void __fastcall transformOtherAttackOnHitHooked(game::CBatAttackTransformOther* thisptr,
                                                int /*%edx*/,
                                                game::IMidgardObjectMap* objectMap,
                                                game::BattleMsgData* battleMsgData,
                                                game::CMidgardID* targetUnitId,
                                                game::BattleAttackInfo** attackInfo)
{
    using namespace game;

    const auto& fn = gameFunctions();

    CMidgardID targetGroupId{emptyId};
    fn.getAllyOrEnemyGroupId(&targetGroupId, battleMsgData, targetUnitId, true);

    const auto targetPosition = fn.getUnitPositionInGroup(objectMap, &targetGroupId, targetUnitId);

    const CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);
    const CMidgardID targetUnitImplId{targetUnit->unitImpl->id};

    auto attackId = &thisptr->attack->id;

    CMidgardID transformImplId{emptyId};
    fn.getSummonUnitImplIdByAttack(&transformImplId, attackId, targetPosition,
                                   isUnitSmall(targetUnit));
    if (transformImplId == emptyId)
        return;

    if (userSettings().leveledTransformOtherAttack) {
        const auto& global = GlobalDataApi::get();
        auto globalData = *global.getGlobalData();

        auto transformImpl = static_cast<TUsUnitImpl*>(
            global.findById(globalData->units, &transformImplId));

        const CMidUnit* unit = fn.findUnitById(objectMap, &thisptr->unitId);
        const auto transformLevel = getTransformOtherLevel(unit, targetUnit, transformImpl,
                                                           objectMap, &thisptr->unitOrItemId,
                                                           battleMsgData);

        CUnitGenerator* unitGenerator = globalData->unitGenerator;

        CMidgardID leveledImplId{transformImplId};
        unitGenerator->vftable->generateUnitImplId(unitGenerator, &leveledImplId, &transformImplId,
                                                   transformLevel);

        unitGenerator->vftable->generateUnitImpl(unitGenerator, &leveledImplId);

        transformImplId = leveledImplId;
    }

    const auto targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);
    bool prevAttackTwice = targetSoldier && targetSoldier->vftable->getAttackTwice(targetSoldier);

    const auto& visitors = VisitorApi::get();
    visitors.transformUnit(targetUnitId, &transformImplId, true, objectMap, 1);

    updateAttackCountAfterTransformation(battleMsgData, targetUnit, prevAttackTwice);

    BattleAttackUnitInfo info{};
    info.unitId = *targetUnitId;
    info.unitImplId = targetUnitImplId;
    BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);

    const auto& battle = BattleMsgDataApi::get();
    battle.removeTransformStatuses(targetUnitId, battleMsgData);
    battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Transform, true);
    if (thisptr->attack->vftable->getInfinite(thisptr->attack))
        battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::TransformLong, true);

    auto targetUnitInfo = battle.getUnitInfoById(battleMsgData, targetUnitId);
    if (targetUnitInfo)
        targetUnitInfo->transformAppliedRound = battleMsgData->currentRound;

    battle.setUnitHp(battleMsgData, targetUnitId, targetUnit->currentHp);
}

} // namespace hooks
