/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/Rapthos/Experimental-version)
 * Copyright (C) 2026 Rapthos.
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

#include "giveattackhooks.h"
#include "attack.h"
#include "batattackgiveattack.h"
#include "battleattackinfo.h"
#include "battlemsgdata.h"
#include "game.h"
#include "immunecat.h"
#include "midunit.h"
#include "ussoldier.h"
#include "usunit.h"

namespace hooks {

    bool __fastcall giveAttackCanPerformHooked(game::CBatAttackGiveAttack* thisptr,
                                               int /*%edx*/,
                                               game::IMidgardObjectMap* objectMap,
                                               game::BattleMsgData* battleMsgData,
                                               game::CMidgardID* unitId)
    {
        using namespace game;
    
        if (*unitId == emptyId || *unitId == invalidId) {
            return false;
        }
    
        CMidgardID targetGroupId{};
        thisptr->vftable->getTargetGroupId(thisptr, &targetGroupId, battleMsgData);
    
        auto& fn = gameFunctions();
        CMidgardID unitGroupId{};
        fn.getAllyOrEnemyGroupId(&unitGroupId, battleMsgData, unitId, true);
    
        if (targetGroupId != unitGroupId) {
            // Do not allow to give additional attacks to enemies
            return false;
        }
    
        if (*unitId == thisptr->unitId1) {
            // Do not allow to give additional attacks to self
            return false;
        }
    
        CMidUnit* unit = fn.findUnitById(objectMap, unitId);
        if (!unit) {
            return false;
        }
    
        auto soldier = fn.castUnitImplToSoldier(unit->unitImpl);
    
        auto attack = soldier->vftable->getAttackById(soldier);
        const auto attackClass = attack->vftable->getAttackClass(attack);
    
        const auto& attackCategories = AttackClassCategories::get();
    
        if (attackClass->id == attackCategories.giveAttack->id) {
            // Do not allow to buff other units with this attack type
            return false;
        }
    
        auto secondAttack = soldier->vftable->getSecondAttackById(soldier);
        if (!secondAttack) {
            return true;
        }
    
        const auto secondAttackClass = secondAttack->vftable->getAttackClass(secondAttack);
        // Do not allow to buff other units with this attack type as their second attack
        return secondAttackClass->id != attackCategories.giveAttack->id;
    }

    void __fastcall giveAttackOnHitHooked(game::CBatAttackGiveAttack* thisptr,
                                          int /*%edx*/,
                                          game::IMidgardObjectMap* objectMap,
                                          game::BattleMsgData* battleMsgData,
                                          game::CMidgardID* targetUnitId,
                                          game::BattleAttackInfo** attackInfo)
    {
        using namespace game;

        int usedSlots = 0;
        for (int i = 0; i < 13; i++) {
            if (battleMsgData->turnsOrder[i].unitId != invalidId)
                usedSlots++;
        }

        if (usedSlots >= 13)
            return;

        CMidUnit* unit = gameFunctions().findUnitById(objectMap, targetUnitId);
        IUsSoldier* soldier = gameFunctions().castUnitImplToSoldier(unit->unitImpl);
        bool hasDoubleAttack = soldier->vftable->getAttackTwice(soldier);

        BattleMsgDataApi::get().giveAttack(battleMsgData, targetUnitId, hasDoubleAttack ? 2 : 1, 1);

        BattleAttackUnitInfo info{};
        info.unitId = unit->id;
        info.unitImplId = unit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

    bool __fastcall giveAttackIsImmuneHooked(game::CBatAttackGiveAttack* thisptr,
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

        IAttack* attack = fn.getAttackById(objectMap, &thisptr->unitId2, thisptr->attackNumber, false);

        if (!attack) {
            return false;
        }

        const CMidUnit* targetUnit = fn.findUnitById(objectMap, unitId);
        if (!targetUnit) {
            return false;
        }

        const IUsSoldier* targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);
        const LAttackClass* attackClass = attack->vftable->getAttackClass(attack);
        const LImmuneCat* immuneCat = targetSoldier->vftable->getImmuneByAttackClass(targetSoldier,
                                                                                     attackClass);
        if (immuneCat->id == immuneCategories.once->id) {
            bool hasWard = battleApi.isUnitAttackClassWardRemoved(battleMsgData, unitId,
                                                                  attackClass);
            if (!hasWard) {
                battleApi.removeUnitAttackClassWard(battleMsgData, unitId, attackClass);
                return true;
            }
            return false;
        }

        return immuneCat->id == immuneCategories.always->id;
    }

}
