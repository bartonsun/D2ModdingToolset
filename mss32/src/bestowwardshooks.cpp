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

#include <fmt/format.h>
#include <fstream>
#include <unitview.h>
#include <unitmodifier.h>
#include <modifierview.h>
#include <battlemsgdataview.h>
#include "scripts.h"
#include <sol/sol.hpp>
#include <attackparams.h>
#include <hooks.h>

namespace hooks {

static bool canPerformSecondaryAttack(game::CBatAttackBestowWards* thisptr,
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

bool __fastcall bestowWardsAttackCanPerformHooked(game::CBatAttackBestowWards* thisptr,
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
           || canApplyAnyModifier(thisptr->attackImpl, objectMap, battleMsgData, targetUnitId)
           || canPerformSecondaryAttack(thisptr, objectMap, battleMsgData, targetUnitId);
}

void __fastcall bestowWardsAttackOnHitHooked(game::CBatAttackBestowWards* thisptr,
                                             int /*%edx*/,
                                             game::IMidgardObjectMap* objectMap,
                                             game::BattleMsgData* battleMsgData,
                                             game::CMidgardID* targetUnitId,
                                             game::BattleAttackInfo** attackInfo)
{
    using namespace game;

    static const auto& battleApi = BattleMsgDataApi::get();
    static const auto& attackClassCategories = AttackClassCategories::get();
    static const auto& immuneCategories = ImmuneCategories::get();
    static const auto& fn = gameFunctions();
    static const auto& attackInfoApi = BattleAttackInfoApi::get();

    auto targetUnit = static_cast<CMidUnit*>(
        objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

    if (!targetUnit) {
        return;
    }

    auto* unitAttacker = game::gameFunctions().findUnitById(objectMap, &thisptr->unitId);
    const auto attack = thisptr->attackImpl;

    bindings::AttackHitParamsView params;
    params.attacker = unitAttacker;
    params.target = targetUnit;
    params.attack = attack;
    params.attackClass = "BestowWard";

    if (unitCanBeModified(battleMsgData, targetUnitId)) {
        const auto wards = attack->vftable->getWards(attack);

        for (const CMidgardID* modifierId = wards->bgn; modifierId != wards->end; ++modifierId) {
            if (canApplyModifier(battleMsgData, targetUnit, modifierId)) {
                params.modifierIds.push_back(modifierId);
            }
        }
    }

    bool healResistance = false;
    const IUsSoldier* targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);

    if (targetSoldier) {
        const LAttackClass* attackClass = attackClassCategories.heal;
        const LImmuneCat* immuneCat = targetSoldier->vftable->getImmuneByAttackClass(targetSoldier,
                                                                                     attackClass);

        if (immuneCat->id == immuneCategories.once->id) {
            healResistance = !battleApi.isUnitAttackClassWardRemoved(battleMsgData, targetUnitId,
                                                                     attackClass);
        } else if (immuneCat->id == immuneCategories.always->id) {
            healResistance = true;
        }
    }

    int qtyHeal = computeBoostedHeal(&thisptr->unitId, battleMsgData, attack->vftable->getQtyHeal(attack));

    params.heal = qtyHeal;

    callLuaAttackHook(objectMap, battleMsgData, params);

    if (params.miss) {
        addToBattleAttackInfo(*attackInfo, targetUnit, 0, 0, true);
        return;
    }

    for (const CMidgardID* modifierId : params.modifierIds) {
        if (!applyModifier(&thisptr->unitId, battleMsgData, targetUnit, modifierId)) {
            break;
        }
    }

    if (targetSoldier && healResistance) {
        const LAttackClass* attackClass = attackClassCategories.heal;
        const LImmuneCat* immuneCat = targetSoldier->vftable->getImmuneByAttackClass(targetSoldier,
                                                                                     attackClass);

        if (immuneCat->id == immuneCategories.once->id) {
            bool wasResistant = !battleApi.isUnitAttackClassWardRemoved(battleMsgData, targetUnitId,
                                                                        attackClass);
            if (wasResistant) {
                battleApi.removeUnitAttackClassWard(battleMsgData, targetUnitId, attackClass);
            }
        }
    }

    int qtyHealed = 0;
    if (!healResistance && battleApi.unitCanBeHealed(objectMap, battleMsgData, targetUnitId) && params.heal > 0) {
        qtyHealed = heal(objectMap, battleMsgData, targetUnit, params.heal);
    }

    addToBattleAttackInfo(*attackInfo, targetUnit, qtyHealed);
}

bool __fastcall bestowWardsMethod15Hooked(game::CBatAttackBestowWards* thisptr,
                                          int /*%edx*/,
                                          game::BattleMsgData* battleMsgData)
{
    return true;
}

bool __fastcall bestowWardsMethod14Hooked(game::CBatAttackBestowWards* thisptr,
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

    static const auto& battleApi = BattleMsgDataApi::get();
    static const auto& immuneCategories = ImmuneCategories::get();
    static const auto& fn = gameFunctions();

    if (*unitId == emptyId || *unitId == invalidId) {
        return false;
    }

    IAttack* attack = fn.getAttackById(objectMap, &thisptr->attackImplUnitId, thisptr->attackNumber, false);

    if (!attack) {
        return false;
    }

    return IsImmuneToAttack(battleMsgData, objectMap, unitId, attack);
}

} // namespace hooks
