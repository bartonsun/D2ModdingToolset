/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/Rapthos/Experimental-version)
 * Copyright (C) 2025 Rapthos.
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

#include "dotattackhooks.h"
#include "attack.h"
#include "batattackblister.h"
#include "batattackblistereffect.h"
#include "batattackfrostbite.h"
#include "batattackfrostbiteeffect.h"
#include "batattackpoison.h"
#include "batattackpoisoneffect.h"
#include "batattackutils.h"
#include "battleattackinfo.h"
#include "game.h"
#include "idvector.h"
#include "hooks.h"
#include "midgardobjectmap.h"
#include "modifierutils.h"
#include "ussoldier.h"
#include "usunit.h"
#include <string>
#include <utils.h>
#include <battlemsgdataview.h>
#include <scenarioview.h>
#include <attackutils.h>
#include <settings.h>
#include "attackimpl.h"
#include "visitors.h"

#include <fmt/format.h>
#include <fstream>
#include "scripts.h"
#include <sol/sol.hpp>

namespace hooks {

    static int computeDotDamage(game::CMidgardID* unitId,
                            game::BattleMsgData* battleMsgData,
                            int baseDamage)
    {

        using namespace game;
        BattleMsgDataApi::Api& battleApi = BattleMsgDataApi::get();

        const auto boostCanAffectDot = gameSettings().extendedBattle.boostCanAffectDot != baseGameSettings().extendedBattle.boostCanAffectDot; 
        const auto lowerCanAffectDot = gameSettings().extendedBattle.lowerCanAffectDot != baseGameSettings().extendedBattle.lowerCanAffectDot; 

        int boost = 0;
        int lower = 0;
        double damage = baseDamage;

        if (boostCanAffectDot) {
            if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl1))
                boost = 1;
            else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl2))
                boost = 2;
            else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl3))
                boost = 3;
            else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl4))
                boost = 4;
        }

        if (lowerCanAffectDot) {
            if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::LowerDamageLvl1))
                lower = 1;
            else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::LowerDamageLvl2))
                lower = 2;
        }
        int sumBoost = hooks::getBoostDamage(boost) - hooks::getLowerDamage(lower);

        int result = int(damage + damage * sumBoost / 100);

        return result;
    }

    bool __fastcall blisterAttackCanPerformHooked(game::CBatAttackBlister* thisptr,
                                            int /*%edx*/,
                                            game::IMidgardObjectMap* objectMap,
                                            game::BattleMsgData* battleMsgData,
                                            game::CMidgardID* targetUnitId)
    {
        using namespace game;

        static const auto& fn = gameFunctions();
        static const auto& battleApi = BattleMsgDataApi::get();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack
                                     != baseGameSettings().extendedBattle.dotDamageCanStack;

        CMidgardID targetGroupId{};
        thisptr->vftable->getTargetGroupId(thisptr, &targetGroupId, battleMsgData);

        CMidgardID unitGroupId{};
        fn.getAllyOrEnemyGroupId(&unitGroupId, battleMsgData, targetUnitId, true);

        if (targetGroupId != unitGroupId) {
            // Can't target allies
            return false;
        }

        if (BattleMsgDataApi::get().getUnitStatus(battleMsgData, targetUnitId, BattleStatus::Retreat))
            return false;

        if (canStack)
            return true;

        if (BattleMsgDataApi::get().getUnitStatus(battleMsgData, targetUnitId,
                                                  BattleStatus::Retreat))
            return false;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);

        int curDamage = targetInfo->blisterAppliedDamage;
        int attackDamage = thisptr->attack->vftable->getQtyDamage(thisptr->attack);
        int computeDamage = computeDotDamage(&thisptr->unitId, battleMsgData, attackDamage);

        if (curDamage > computeDamage)
            return false;

        return true;
    }

    bool __fastcall frostbiteAttackCanPerformHooked(game::CBatAttackFrostbite* thisptr,
                                                    int /*%edx*/,
                                                    game::IMidgardObjectMap* objectMap,
                                                    game::BattleMsgData* battleMsgData,
                                                    game::CMidgardID* targetUnitId)
    {
        using namespace game;

        static const auto& fn = gameFunctions();
        static const auto& battleApi = BattleMsgDataApi::get();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack
                                     != baseGameSettings().extendedBattle.dotDamageCanStack;

        CMidgardID targetGroupId{};
        thisptr->vftable->getTargetGroupId(thisptr, &targetGroupId, battleMsgData);

        CMidgardID unitGroupId{};
        fn.getAllyOrEnemyGroupId(&unitGroupId, battleMsgData, targetUnitId, true);

        if (targetGroupId != unitGroupId) {
            // Can't target allies
            return false;
        }

        if (BattleMsgDataApi::get().getUnitStatus(battleMsgData, targetUnitId,
                                                  BattleStatus::Retreat))
            return false;

        if (canStack)
            return true;

        if (BattleMsgDataApi::get().getUnitStatus(battleMsgData, targetUnitId,
                                                  BattleStatus::Retreat))
            return false;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);

        int curDamage = targetInfo->frostbiteAppliedDamage;
        int attackDamage = thisptr->attack->vftable->getQtyDamage(thisptr->attack);
        int computeDamage = computeDotDamage(&thisptr->unitId, battleMsgData, attackDamage);

        if (curDamage > computeDamage)
            return false;

        return true;
    }

    bool __fastcall poisonAttackCanPerformHooked(game::CBatAttackPoison* thisptr,
                                                 int /*%edx*/,
                                                 game::IMidgardObjectMap* objectMap,
                                                 game::BattleMsgData* battleMsgData,
                                                 game::CMidgardID* targetUnitId)
    {
        using namespace game;

        static const auto& fn = gameFunctions();
        static const auto& battleApi = BattleMsgDataApi::get();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack
                                     != baseGameSettings().extendedBattle.dotDamageCanStack;

        CMidgardID targetGroupId{};
        thisptr->vftable->getTargetGroupId(thisptr, &targetGroupId, battleMsgData);

        CMidgardID unitGroupId{};
        fn.getAllyOrEnemyGroupId(&unitGroupId, battleMsgData, targetUnitId, true);

        if (targetGroupId != unitGroupId) {
            // Can't target allies
            return false;
        }

        if (BattleMsgDataApi::get().getUnitStatus(battleMsgData, targetUnitId,
                                                  BattleStatus::Retreat))
            return false;

        if (canStack)
            return true;

        if (BattleMsgDataApi::get().getUnitStatus(battleMsgData, targetUnitId,
                                                  BattleStatus::Retreat))
            return false;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);

        int curDamage = targetInfo->poisonAppliedDamage;
        int attackDamage = thisptr->attack->vftable->getQtyDamage(thisptr->attack);
        int computeDamage = computeDotDamage(&thisptr->unitId, battleMsgData, attackDamage);

        if (curDamage > computeDamage)
            return false;

        return true;
    }

    void __fastcall blisterAttackOnHitHooked(game::CBatAttackBlister* thisptr,
                                               int /*%edx*/,
                                               game::IMidgardObjectMap* objectMap,
                                               game::BattleMsgData* battleMsgData,
                                               game::CMidgardID* targetUnitId,
                                               game::BattleAttackInfo** attackInfo)
    {
        using namespace game;

        static const auto& battleApi = BattleMsgDataApi::get();
        static const auto& fn = gameFunctions();
        static const auto& idApi = CMidgardIDApi::get();
        static const auto& settings = gameSettings();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack != baseGameSettings().extendedBattle.dotDamageCanStack;
        static const int maxDotDamage = gameSettings().extendedBattle.maxDotDamage;
        static const bool usePower = gameSettings().extendedBattle.longEffectsUsesPower != baseGameSettings().extendedBattle.longEffectsUsesPower;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        if (!targetUnit)
            return;

        const auto attack = thisptr->attack;

        int damage = attack->vftable->getQtyDamage(attack);
        if (idApi.getType(&thisptr->id2) != IdType::Item) {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        bool isLong = attack->vftable->getInfinite(attack);

        CMidUnit* unitAttacker = fn.findUnitById(objectMap, &thisptr->unitId);

        bindings::AttackHitParamsView params;
        params.attacker = unitAttacker;
        params.target = targetUnit;
        params.attack = attack;
        params.attackClass = "Blister";
        params.damage = damage;
        params.isLong = isLong;
        params.miss = false;

        callLuaAttackHook(objectMap, battleMsgData, params);

        if (params.miss) {
            addToBattleAttackInfo(*attackInfo, targetUnit, 0, 0, true);
            return;
        }

        int curDamage = targetInfo->blisterAppliedDamage;

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Blister, false);
        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BlisterLong, false);

        if (canStack) {
            targetInfo->blisterAppliedDamage = std::clamp(curDamage + params.damage, 0,
                                                          maxDotDamage);
        }
        else
        {
            targetInfo->blisterAppliedDamage = std::clamp(params.damage, 0, maxDotDamage);
        }

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Blister, true);

        if (usePower)
        {
            int level = thisptr->attack->vftable->getLevel(thisptr->attack);
            targetInfo->blisterAppliedRound = level > 0 ? level : 1;
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BlisterLong, true);
        }
        else
        {
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BlisterLong,
                                    params.isLong ? true : false);
            targetInfo->blisterAppliedRound = battleMsgData->currentRound;
        }

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        addToBattleAttackInfo(*attackInfo, targetUnit);
    }
    
    void __fastcall frostbiteAttackOnHitHooked(game::CBatAttackFrostbite* thisptr,
                                               int /*%edx*/,
                                               game::IMidgardObjectMap* objectMap,
                                               game::BattleMsgData* battleMsgData,
                                               game::CMidgardID* targetUnitId,
                                               game::BattleAttackInfo** attackInfo)
    {
        using namespace game;

        static const auto& battleApi = BattleMsgDataApi::get();
        static const auto& fn = gameFunctions();
        static const auto& idApi = CMidgardIDApi::get();
        static const auto& settings = gameSettings();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack
                                     != baseGameSettings().extendedBattle.dotDamageCanStack;
        static const int maxDotDamage = gameSettings().extendedBattle.maxDotDamage;
        static const bool usePower = gameSettings().extendedBattle.longEffectsUsesPower
                                     != baseGameSettings().extendedBattle.longEffectsUsesPower;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        if (!targetUnit)
            return;

        const auto attack = thisptr->attack;

        int damage = attack->vftable->getQtyDamage(attack);
        if (idApi.getType(&thisptr->id2) != IdType::Item) {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        bool isLong = attack->vftable->getInfinite(attack);

        CMidUnit* unitAttacker = fn.findUnitById(objectMap, &thisptr->unitId);

        bindings::AttackHitParamsView params;
        params.attacker = unitAttacker;
        params.target = targetUnit;
        params.attack = attack;
        params.attackClass = "Frostbite";
        params.damage = damage;
        params.isLong = isLong;
        params.miss = false;

        callLuaAttackHook(objectMap, battleMsgData, params);

        if (params.miss) {
            addToBattleAttackInfo(*attackInfo, targetUnit, 0, 0, true);
            return;
        }

        int curDamage = targetInfo->frostbiteAppliedDamage;

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Frostbite, false);
        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::FrostbiteLong, false);

        if (canStack) {
            targetInfo->frostbiteAppliedDamage = std::clamp(curDamage + params.damage, 0,
                                                            maxDotDamage);
        } else {
            targetInfo->frostbiteAppliedDamage = std::clamp(params.damage, 0, maxDotDamage);
        }

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Frostbite, true);

        if (usePower) {
            int level = thisptr->attack->vftable->getLevel(thisptr->attack);
            targetInfo->frostbiteAppliedRound = level > 0 ? level : 1;
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::FrostbiteLong, true);
        } else {
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::FrostbiteLong,
                                    params.isLong ? true : false);
            targetInfo->frostbiteAppliedRound = battleMsgData->currentRound;
        }

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        addToBattleAttackInfo(*attackInfo, targetUnit);
    }

    void __fastcall poisonAttackOnHitHooked(game::CBatAttackPoison* thisptr,
                                            int /*%edx*/,
                                            game::IMidgardObjectMap* objectMap,
                                            game::BattleMsgData* battleMsgData,
                                            game::CMidgardID* targetUnitId,
                                            game::BattleAttackInfo** attackInfo)
    {
        using namespace game;

        static const auto& battleApi = BattleMsgDataApi::get();
        static const auto& fn = gameFunctions();
        static const auto& idApi = CMidgardIDApi::get();
        static const auto& settings = gameSettings();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack
                                     != baseGameSettings().extendedBattle.dotDamageCanStack;
        static const int maxDotDamage = gameSettings().extendedBattle.maxDotDamage;
        static const bool usePower = gameSettings().extendedBattle.longEffectsUsesPower
                                     != baseGameSettings().extendedBattle.longEffectsUsesPower;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        if (!targetUnit)
            return;

        const auto attack = thisptr->attack;

        int damage = attack->vftable->getQtyDamage(attack);
        if (idApi.getType(&thisptr->id2) != IdType::Item) {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        bool isLong = attack->vftable->getInfinite(attack);

        CMidUnit* unitAttacker = fn.findUnitById(objectMap, &thisptr->unitId);

        bindings::AttackHitParamsView params;
        params.attacker = unitAttacker;
        params.target = targetUnit;
        params.attack = attack;
        params.attackClass = "Poison";
        params.damage = damage;
        params.isLong = isLong;
        params.miss = false;

        callLuaAttackHook(objectMap, battleMsgData, params);

        if (params.miss) {
            addToBattleAttackInfo(*attackInfo, targetUnit, 0, 0, true);
            return;
        }

        int curDamage = targetInfo->poisonAppliedDamage;

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Poison, false);
        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::PoisonLong, false);

        if (canStack) {
            targetInfo->poisonAppliedDamage = std::clamp(curDamage + params.damage, 0,
                                                         maxDotDamage);
        } else {
            targetInfo->poisonAppliedDamage = std::clamp(params.damage, 0, maxDotDamage);
        }

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Poison, true);

        if (usePower) {
            int level = thisptr->attack->vftable->getLevel(thisptr->attack);
            targetInfo->poisonAppliedRound = level > 0 ? level : 1;
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::PoisonLong, true);
        } else {
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::PoisonLong,
                                    params.isLong ? true : false);
            targetInfo->poisonAppliedRound = battleMsgData->currentRound;
        }

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        addToBattleAttackInfo(*attackInfo, targetUnit);
    }

    //EFFECT
    void __fastcall blisterEffectOnHitHooked(game::CBatAttackBlisterEffect* thisptr,
                                             int /*%edx*/,
                                             game::IMidgardObjectMap* objectMap,
                                             game::BattleMsgData* battleMsgData,
                                             game::CMidgardID* targetUnitId,
                                             game::BattleAttackInfo** attackInfo)
    {
        using namespace game;

        static const auto& battleApi = BattleMsgDataApi::get();
        static const auto& fn = gameFunctions();
        static const auto& visitorApi = VisitorApi::get();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack
                                     != baseGameSettings().extendedBattle.dotDamageCanStack;
        static const int maxDotDamage = gameSettings().extendedBattle.maxDotDamage;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        int damage = std::clamp(targetInfo->blisterAppliedDamage, 0, maxDotDamage);

        CMidUnit* unitAttacker = fn.findUnitById(objectMap, &thisptr->unitId);

        bindings::AttackHitParamsView params;
        params.attacker = unitAttacker;
        params.target = targetUnit;
        params.attack = nullptr;
        params.attackClass = "BlisterEffect";
        params.damage = damage;

        callLuaAttackHook(objectMap, battleMsgData, params);

        visitorApi.changeUnitHp(targetUnitId, -params.damage, objectMap, 1);

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        battleApi.setUnitHp(battleMsgData, targetUnitId, targetUnit->currentHp);

        if (canStack)
        {
            targetInfo->blisterAppliedDamage -= params.damage / 3;
        }

        addToBattleAttackInfo(*attackInfo, targetUnit, params.damage);
    }

    void __fastcall frostbiteEffectOnHitHooked(game::CBatAttackFrostbiteEffect* thisptr,
                                               int /*%edx*/,
                                               game::IMidgardObjectMap* objectMap,
                                               game::BattleMsgData* battleMsgData,
                                               game::CMidgardID* targetUnitId,
                                               game::BattleAttackInfo** attackInfo)
    {
        using namespace game;

        static const auto& battleApi = BattleMsgDataApi::get();
        static const auto& fn = gameFunctions();
        static const auto& visitorApi = VisitorApi::get();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack
                                     != baseGameSettings().extendedBattle.dotDamageCanStack;
        static const int maxDotDamage = gameSettings().extendedBattle.maxDotDamage;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        int damage = std::clamp(targetInfo->frostbiteAppliedDamage, 0, maxDotDamage);

        CMidUnit* unitAttacker = fn.findUnitById(objectMap, &thisptr->unitId);

        bindings::AttackHitParamsView params;
        params.attacker = unitAttacker;
        params.target = targetUnit;
        params.attack = nullptr;
        params.attackClass = "FrostbiteEffect";
        params.damage = damage;

        callLuaAttackHook(objectMap, battleMsgData, params);

        visitorApi.changeUnitHp(targetUnitId, -params.damage, objectMap, 1);

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        battleApi.setUnitHp(battleMsgData, targetUnitId, targetUnit->currentHp);

        if (canStack) {
            targetInfo->frostbiteAppliedDamage -= params.damage / 3;
        }

        addToBattleAttackInfo(*attackInfo, targetUnit, params.damage);
    }

    void __fastcall poisonEffectOnHitHooked(game::CBatAttackPoisonEffect* thisptr,
                                            int /*%edx*/,
                                            game::IMidgardObjectMap* objectMap,
                                            game::BattleMsgData* battleMsgData,
                                            game::CMidgardID* targetUnitId,
                                            game::BattleAttackInfo** attackInfo)
    {
        using namespace game;

        static const auto& battleApi = BattleMsgDataApi::get();
        static const auto& fn = gameFunctions();
        static const auto& visitorApi = VisitorApi::get();

        static const bool canStack = gameSettings().extendedBattle.dotDamageCanStack
                                     != baseGameSettings().extendedBattle.dotDamageCanStack;
        static const int maxDotDamage = gameSettings().extendedBattle.maxDotDamage;

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        int damage = std::clamp(targetInfo->poisonAppliedDamage, 0, maxDotDamage);

        CMidUnit* unitAttacker = fn.findUnitById(objectMap, &thisptr->unitId);

        bindings::AttackHitParamsView params;
        params.attacker = unitAttacker;
        params.target = targetUnit;
        params.attack = nullptr;
        params.attackClass = "PoisonEffect";
        params.damage = damage;

        callLuaAttackHook(objectMap, battleMsgData, params);

        visitorApi.changeUnitHp(targetUnitId, -params.damage, objectMap, 1);

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        battleApi.setUnitHp(battleMsgData, targetUnitId, targetUnit->currentHp);

        if (canStack) {
            targetInfo->poisonAppliedDamage -= params.damage / 3;
        }

        addToBattleAttackInfo(*attackInfo, targetUnit, params.damage);
    }

    } // namespace hooks
