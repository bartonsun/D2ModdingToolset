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

#include "bestowwardshooks.h"
#include "attack.h"
#include "batattackbestowwards.h"
#include "batattackutils.h"
#include "battleattackinfo.h"
#include "game.h"
#include "idvector.h"
#include "midgardobjectmap.h"
#include "modifierutils.h"
#include "usunit.h"

#include "ussoldier.h"

namespace hooks {

bool canPerformSecondaryAttack(game::CBatAttackBestowWards* thisptr,
                               game::IMidgardObjectMap* objectMap,
                               game::BattleMsgData* battleMsgData,
                               game::CMidgardID* targetUnitId)
{
    using namespace game;

    const auto& fn = gameFunctions();
    if (thisptr->attackImplMagic == -1)
        thisptr->attackImplMagic = fn.getAttackImplMagic(objectMap, &thisptr->attackImplUnitId, 0);

    if (thisptr->attackImplMagic <= 1)
        return false;

    if (thisptr->attackNumber != 1)
        return false;

    if (CMidgardIDApi::get().getType(&thisptr->attackImplUnitId) == IdType::Item)
        return false;

    if (!thisptr->attack2Initialized) {
        thisptr->attack2Impl = fn.getAttackById(objectMap, &thisptr->attackImplUnitId,
                                                thisptr->attackNumber + 1, true);
        thisptr->attack2Initialized = true;
    }

    if (thisptr->attack2Impl == nullptr)
        return false;

    const auto attack2 = thisptr->attack2Impl;
    const auto attack2Class = attack2->vftable->getAttackClass(attack2);
    const auto batAttack2 = fn.createBatAttack(objectMap, battleMsgData, &thisptr->unitId,
                                               &thisptr->attackImplUnitId,
                                               thisptr->attackNumber + 1, attack2Class, false);

    const auto& battle = BattleMsgDataApi::get();
    bool result = battle.canPerformAttackOnUnitWithStatusCheck(objectMap, battleMsgData, batAttack2,
                                                               targetUnitId);
    batAttack2->vftable->destructor(batAttack2, true);
    return result;
}

bool __fastcall bestowWardsAttackCanPerformHooked(game::CBatAttackBestowWards* thisptr,
                                                  int /*%edx*/,
                                                  game::IMidgardObjectMap* objectMap,
                                                  game::BattleMsgData* battleMsgData,
                                                  game::CMidgardID* targetUnitId)
{
    using namespace game;

    const auto& fn = gameFunctions();

    if (*targetUnitId == emptyId || *targetUnitId == invalidId) {
        return false;
    }

    CMidgardID targetGroupId{};
    thisptr->vftable->getTargetGroupId(thisptr, &targetGroupId, battleMsgData);

    CMidgardID targetUnitGroupId{};
    gameFunctions().getAllyOrEnemyGroupId(&targetUnitGroupId, battleMsgData, targetUnitId, true);

    CMidUnit* unit = fn.findUnitById(objectMap, targetUnitId);
    if (!unit) {
        return false;
    }

    auto soldier = fn.castUnitImplToSoldier(unit->unitImpl);

    if (targetUnitGroupId != targetGroupId)
        return false;

    if (canHeal(thisptr->attackImpl, objectMap, battleMsgData, targetUnitId))
        return true;

    if (canApplyAnyModifier(thisptr->attackImpl, objectMap, battleMsgData, targetUnitId))
        return true;

    return canPerformSecondaryAttack(thisptr, objectMap, battleMsgData, targetUnitId);
}

void __fastcall bestowWardsAttackOnHitHooked(game::CBatAttackBestowWards* thisptr,
                                             int /*%edx*/,
                                             game::IMidgardObjectMap* objectMap,
                                             game::BattleMsgData* battleMsgData,
                                             game::CMidgardID* targetUnitId,
                                             game::BattleAttackInfo** attackInfo)
{
    using namespace game;

    auto& fn = gameFunctions();

    auto targetUnit = static_cast<CMidUnit*>(
        objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

    if (unitCanBeModified(battleMsgData, targetUnitId)) {
        const auto attack = thisptr->attackImpl;
        const auto wards = attack->vftable->getWards(attack);
        for (const CMidgardID* modifierId = wards->bgn; modifierId != wards->end; modifierId++) {
            if (canApplyModifier(battleMsgData, targetUnit, modifierId)) {
                if (!applyModifier(&thisptr->unitId, battleMsgData, targetUnit, modifierId))
                    break;
            }
        }
    }

    // Prevents healing if the target has Heal resistance
    bool healResistance = false;

    const IUsSoldier* targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);
    const LAttackClass* attackClass = AttackClassCategories::get().heal;
    const LImmuneCat* immuneCatC = targetSoldier->vftable->getImmuneByAttackClass(targetSoldier,
                                                                                  attackClass);

    if (immuneCatC->id == ImmuneCategories::get().once->id) {
        healResistance = !BattleMsgDataApi::get().isUnitAttackClassWardRemoved(battleMsgData,
                                                                               targetUnitId,
                                                                               attackClass);
        if (healResistance)
            BattleMsgDataApi::get().removeUnitAttackClassWard(battleMsgData, targetUnitId,
                                                              attackClass);
        healResistance = true;
    } else if (immuneCatC->id == ImmuneCategories::get().always->id) {
        healResistance = true;
    }

    int qtyHealed = 0;
    if (!healResistance
        && BattleMsgDataApi::get().unitCanBeHealed(objectMap, battleMsgData, targetUnitId)) {
        const auto attack = thisptr->attackImpl;
        const int qtyHeal = computeBoostedHeal(&thisptr->unitId, battleMsgData,
                                               attack->vftable->getQtyHeal(attack));
        if (qtyHeal > 0)
            qtyHealed = heal(objectMap, battleMsgData, targetUnit, qtyHeal);
    }

    BattleAttackUnitInfo info{};
    info.unitId = targetUnit->id;
    info.unitImplId = targetUnit->unitImpl->id;
    info.damage = qtyHealed;
    BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
}

bool __fastcall bestowWardsMethod15Hooked(game::CBatAttackBestowWards* thisptr,
                                          int /*%edx*/,
                                          game::BattleMsgData* battleMsgData)
{
    return true;
}

bool __fastcall bestowWardsAttackIsImmuneHooked(game::CBatAttackBestowWards* thisptr,
                                                int /*%edx*/,
                                                game::IMidgardObjectMap* objectMap,
                                                game::BattleMsgData* battleMsgData,
                                                game::CMidgardID* unitId)
{
    using namespace game;

    const auto& battle = BattleMsgDataApi::get();

    const auto& fn = gameFunctions();
    IAttack* attack = fn.getAttackById(objectMap, &thisptr->attackImplUnitId, thisptr->attackNumber,
                                       false);

    if (attack == NULL)
        return false;

    // Only for "heal/ressurect" combination
    bool isDead = BattleMsgDataApi::get().getUnitStatus(battleMsgData, unitId, BattleStatus::Dead);
    if (isDead && canPerformSecondaryAttack(thisptr, objectMap, battleMsgData, unitId))
        return false;

    const CMidUnit* targetUnit = fn.findUnitById(objectMap, unitId);
    if (!targetUnit)
        return false;

    const IUsSoldier* targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);

    const LAttackClass* attackClass = attack->vftable->getAttackClass(attack);
    const LImmuneCat* immuneCatC = targetSoldier->vftable->getImmuneByAttackClass(targetSoldier,
                                                                                  attackClass);

    bool result = false;
    if (immuneCatC->id == ImmuneCategories::get().once->id) {
        result = !battle.isUnitAttackClassWardRemoved(battleMsgData, unitId, attackClass);
        if (result)
            battle.removeUnitAttackClassWard(battleMsgData, unitId, attackClass);
    } else if (immuneCatC->id == ImmuneCategories::get().always->id) {
        result = true;
    }

    return result;
}

} // namespace hooks
