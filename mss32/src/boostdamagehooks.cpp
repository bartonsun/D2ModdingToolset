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

#include "batattackboostdamage.h"
#include "attack.h"
#include "battleattackinfo.h"
#include "battlemsgdata.h"
#include "game.h"
#include "boostdamagehooks.h"
#include "immunecat.h"
#include "midunit.h"
#include "midgardobjectmap.h"
#include "ussoldier.h"
#include "usunit.h"

namespace hooks {

    static int getBoostLevel(game::BattleMsgData* battleMsgData, game::CMidgardID* unitId)
    {
        using namespace game;
        auto& battle = BattleMsgDataApi::get();
    
        if (battle.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl1))
            return 1;
        if (battle.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl2))
            return 2;
        if (battle.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl3))
            return 3;
        if (battle.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl4))
            return 4;
    
        return 0;
    }

    bool __fastcall boostDamageCanPerformHooked(game::CBatAttackBoostDamage* thisptr,
                                                int /*%edx*/,
                                                game::IMidgardObjectMap* objectMap,
                                                game::BattleMsgData* battleMsgData,
                                                game::CMidgardID* unitId)
    {
        using namespace game;
    
        static const auto& fn = gameFunctions();
        static const auto& attackCategories = AttackClassCategories::get();
    
        if (*unitId == emptyId || *unitId == invalidId) {
            return false;
        }
    
        const CMidUnit* unit = fn.findUnitById(objectMap, unitId);
        if (!unit) {
            return false;
        }
    
        auto soldier = fn.castUnitImplToSoldier(unit->unitImpl);
    
        if (!soldier)
            return false;
    
        const IAttack* attack = soldier->vftable->getAttackById(soldier);
        const LAttackClass* attackClass = attack->vftable->getAttackClass(attack);
    
        if (attackClass->id == attackCategories.boostDamage->id) {
            return false;
        }
    
        CMidgardID unitGroupId{};
        gameFunctions().getAllyOrEnemyGroupId(&unitGroupId, battleMsgData, unitId, true);
    
        CMidgardID targetGroupId{};
        thisptr->vftable->getTargetGroupId(thisptr, &targetGroupId, battleMsgData);
    
        if (targetGroupId != unitGroupId) {
            return false;
        }
    
        const int curLvl = getBoostLevel(battleMsgData, unitId);
    
        return curLvl <= thisptr->attackImpl->vftable->getLevel(thisptr->attackImpl);
    }

    void __fastcall boostDamageOnHitHooked(game::CBatAttackBoostDamage* thisptr,
                                           int /*%edx*/,
                                           game::IMidgardObjectMap* objectMap,
                                           game::BattleMsgData* battleMsgData,
                                           game::CMidgardID* targetUnitId,
                                           game::BattleAttackInfo** attackInfo)
    {
        using namespace game;
        auto& battle = BattleMsgDataApi::get();

        auto* targetUnit = static_cast<CMidUnit*>(
            objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

        if (!targetUnit) {
            return;
        }

        auto* attack = thisptr->attackImpl;

        for (int i = 0; i < 4; ++i) {
            battle.setUnitStatus(battleMsgData, targetUnitId,
                                 static_cast<BattleStatus>((int)BattleStatus::BoostDamageLvl1 + i),
                                 false);
        }

        const int level = attack->vftable->getLevel(attack);
        if (level >= 1 && level <= 4) {
            auto status = static_cast<BattleStatus>((int)BattleStatus::BoostDamageLvl1
                                                    + (level - 1));
            battle.setUnitStatus(battleMsgData, targetUnitId, status, true);
        }

        const bool infinite = attack->vftable->getInfinite(attack);
        battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BoostDamageLong, infinite);

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

    bool __fastcall boostDamageIsImmuneHooked(game::CBatAttackBoostDamage* thisptr,
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

        IAttack* attack = fn.getAttackById(objectMap, &thisptr->attackImplUnitId,
                                           thisptr->attackNumber,
                                           false);

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
