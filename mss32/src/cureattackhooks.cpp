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

#include "cureattackhooks.h"
#include "batattackcure.h"
#include "attack.h"
#include "battleattackinfo.h"
#include "battlemsgdata.h"
#include "game.h"
#include "immunecat.h"
#include "midunit.h"
#include "ussoldier.h"
#include "usunit.h"
#include "settings.h"
#include <batattackutils.h>
#include <attackparams.h>
#include <hooks.h>

namespace hooks {

    bool __fastcall cureAttackCanPerformHooked(game::CBatAttackCure* thisptr,
                                               int /*%edx*/,
                                               game::IMidgardObjectMap* objectMap,
                                               game::BattleMsgData* battleMsgData,
                                               game::CMidgardID* targetUnitId)
    {
        using namespace game;
        
        static const auto& battleApi = BattleMsgDataApi::get();

        if (*targetUnitId == emptyId || *targetUnitId == invalidId) {
            return false;
        }
    
        CMidgardID targetGroupId{};
        thisptr->vftable->getTargetGroupId(thisptr, &targetGroupId, battleMsgData);
    
        auto& fn = gameFunctions();
        CMidgardID unitGroupId{};
        fn.getAllyOrEnemyGroupId(&unitGroupId, battleMsgData, targetUnitId, true);
    
        if (targetGroupId != unitGroupId) {
            // Do not allow to cure enemies
            return false;
        }
        
        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);

        if (targetInfo->unitHp <= 0)
            return false;

        return true;
    }

    void __fastcall cureAttackOnHitHooked(game::CBatAttackCure* thisptr,
                                          int /*%edx*/,
                                          game::IMidgardObjectMap* objectMap,
                                          game::BattleMsgData* battleMsgData,
                                          game::CMidgardID* targetUnitId,
                                          game::BattleAttackInfo** attackInfo)
    {
        using namespace game;
    
        static const auto& fn = gameFunctions();
        static const auto& battleApi = BattleMsgDataApi::get();
    
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);
        if (!targetUnit)
            return;
    
        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        
        auto* unitAttacker = fn.findUnitById(objectMap, &thisptr->unitId);

        IAttack* attack = fn.getAttackById(objectMap, &thisptr->attackImplUnitId,
                                                    thisptr->attackNumber, false);

        bindings::AttackHitParamsView params;
        params.attacker = unitAttacker;
        params.target = targetUnit;
        params.attack = attack;
        params.attackClass = "Cure";
        params.miss = false;

        callLuaAttackHook(objectMap, battleMsgData, params);

        if (params.miss) {
            addToBattleAttackInfo(*attackInfo, targetUnit, 0, 0, true);
            return;
        }

        BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Poison, false);
        BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::PoisonLong,
                                              false);
        BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Blister,
                                              false);
        BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BlisterLong,
                                              false);
        BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Frostbite,
                                              false);
        BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::FrostbiteLong,
                                              false);
    
        targetInfo->poisonAppliedDamage = 0;
        targetInfo->blisterAppliedDamage = 0;
        targetInfo->frostbiteAppliedDamage = 0;
        targetInfo->poisonAppliedRound = 0;
        targetInfo->blisterAppliedRound = 0;
        targetInfo->frostbiteAppliedRound = 0;
    
        if (gameSettings().advancedCure != baseGameSettings().advancedCure) {
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::LowerDamageLvl1, false);
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::LowerDamageLvl2, false);
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::LowerDamageLong, false);
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::LowerInitiative, false);
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::LowerInitiativeLong,
                                    false);
        }
    
        BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Cured, true);
    
        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

    bool __fastcall cureAttackIsImmuneHooked(game::CBatAttackCure* thisptr,
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

    bool __fastcall cureAttackMethod15Hooked(game::CBatAttackCure* thisptr,
                                             int /*%edx*/,
                                             game::BattleMsgData* battleMsgData)
    {
        return true;
    }

} // namespace hooks
