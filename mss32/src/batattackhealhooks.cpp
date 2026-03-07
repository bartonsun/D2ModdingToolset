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

#include "batattackhealhooks.h"
#include "batattackheal.h"
#include "batattackutils.h"
#include "battlemsgdata.h"
#include "game.h"
#include "midgardobjectmap.h"
#include "ussoldier.h"
#include "utils.h"
#include <spdlog/spdlog.h>

#include "usunit.h"
#include "battleattackinfo.h"

namespace hooks {

static bool canPerformSecondaryAttack(game::CBatAttackHeal* thisptr,
                                      game::IMidgardObjectMap* objectMap,
                                      game::BattleMsgData* battleMsgData,
                                      game::CMidgardID* targetUnitId)
{
    using namespace game;

    static const auto& fn = gameFunctions();
    static const auto& battleApi = BattleMsgDataApi::get();
    static const auto& idApi = CMidgardIDApi::get();

    if (thisptr->attackNumber != 1) {
        return false;
    }

    if (thisptr->attackImplMagic == -1) {
        thisptr->attackImplMagic = fn.getAttackImplMagic(objectMap, &thisptr->attackImplUnitId, 0);
    }

    if (thisptr->attackImplMagic <= 1) {
        return false;
    }

    if (idApi.getType(&thisptr->attackImplUnitId) == IdType::Item) {
        return false;
    }

    if (!thisptr->attack2Initialized) {
        thisptr->attack2Impl = fn.getAttackById(objectMap, &thisptr->attackImplUnitId,
                                                thisptr->attackNumber + 1, true);
        thisptr->attack2Initialized = true;
    }

    if (!thisptr->attack2Impl) {
        return false;
    }

    const auto attack2Class = thisptr->attack2Impl->vftable->getAttackClass(thisptr->attack2Impl);
    const auto batAttack2 = fn.createBatAttack(objectMap, battleMsgData, &thisptr->unitId,
                                               &thisptr->attackImplUnitId,
                                               thisptr->attackNumber + 1, attack2Class, false);

    const bool result = battleApi.canPerformAttackOnUnitWithStatusCheck(objectMap, battleMsgData,
                                                                        batAttack2, targetUnitId);
    batAttack2->vftable->destructor(batAttack2, true);

    return result;
}

bool __fastcall healAttackCanPerformHooked(game::CBatAttackHeal* thisptr,
                                                  int /*%edx*/,
                                                  game::IMidgardObjectMap* objectMap,
                                                  game::BattleMsgData* battleMsgData,
                                                  game::CMidgardID* targetUnitId)
{
    using namespace game;

    static const auto& fn = gameFunctions();

    if (*targetUnitId == emptyId || *targetUnitId == invalidId) {
        return false;
    }

    CMidgardID targetGroupId{};
    thisptr->vftable->getTargetGroupId(thisptr, &targetGroupId, battleMsgData);

    CMidgardID targetUnitGroupId{};
    fn.getAllyOrEnemyGroupId(&targetUnitGroupId, battleMsgData, targetUnitId, true);

    if (targetUnitGroupId != targetGroupId) {
        return false;
    }

    CMidUnit* unit = fn.findUnitById(objectMap, targetUnitId);
    if (!unit) {
        return false;
    }

    return canHeal(thisptr->attackImpl, objectMap, battleMsgData, targetUnitId)
           || canPerformSecondaryAttack(thisptr, objectMap, battleMsgData, targetUnitId);
}

void __fastcall healAttackOnHitHooked(game::CBatAttackHeal* thisptr,
                                      int /*%edx*/,
                                      game::IMidgardObjectMap* objectMap,
                                      game::BattleMsgData* battleMsgData,
                                      game::CMidgardID* targetUnitId,
                                      game::BattleAttackInfo** attackInfo)
{
    using namespace game;

    static const auto& fn = gameFunctions();
    static const auto& battleApi = BattleMsgDataApi::get();
    static const auto& attackInfoApi = BattleAttackInfoApi::get();

    auto targetUnit = static_cast<CMidUnit*>(
        objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

    if (!targetUnit) {
        return;
    }

    int qtyHealed = 0;
    if (battleApi.unitCanBeHealed(objectMap, battleMsgData, targetUnitId)) {
        const auto attack = thisptr->attackImpl;
        const int qtyHeal = computeBoostedHeal(&thisptr->unitId, battleMsgData,
                                               attack->vftable->getQtyHeal(attack));

        if (qtyHeal > 0) {
            qtyHealed = heal(objectMap, battleMsgData, targetUnit, qtyHeal);
        }
    }

    BattleAttackUnitInfo info{};
    info.unitId = targetUnit->id;
    info.unitImplId = targetUnit->unitImpl->id;
    info.damage = qtyHealed;
    attackInfoApi.addUnitInfo(&(*attackInfo)->unitsInfo, &info);
}

} // namespace hooks