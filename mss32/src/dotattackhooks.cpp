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

namespace hooks {

    static int computeDotDamage(game::CMidgardID* unitId,
                            game::BattleMsgData* battleMsgData,
                            int baseDamage)
    {

        using namespace game;
        BattleMsgDataApi::Api& battleApi = BattleMsgDataApi::get();

        int boost = 0;
        int lower = 0;
        double damage = baseDamage;

        if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl1))
            boost = 1;
        else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl2))
            boost = 2;
        else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl3))
            boost = 3;
        else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl4))
            boost = 4;

        if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::LowerDamageLvl1))
            lower = 1;
        else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::LowerDamageLvl2))
            lower = 2;

        int sumBoost = hooks::getBoostDamage(boost) - hooks::getLowerDamage(lower);

        int result = int(damage + damage * sumBoost / 100);

        return result;
    }

    bool __fastcall blisterCanPerformHooked(game::CBatAttackBlister* thisptr,
                                            int /*%edx*/,
                                            game::IMidgardObjectMap* objectMap,
                                            game::BattleMsgData* battleMsgData,
                                            game::CMidgardID* targetUnitId)
    {
        using namespace game;
        auto& fn = gameFunctions();

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

        return true;
    }

    bool __fastcall frostbiteCanPerformHooked(game::CBatAttackFrostbite* thisptr,
                                            int /*%edx*/,
                                            game::IMidgardObjectMap* objectMap,
                                            game::BattleMsgData* battleMsgData,
                                            game::CMidgardID* targetUnitId)
    {
        using namespace game;
        auto& fn = gameFunctions();

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

        return true;
    }

    bool __fastcall poisonCanPerformHooked(game::CBatAttackPoison* thisptr,
                                            int /*%edx*/,
                                            game::IMidgardObjectMap* objectMap,
                                            game::BattleMsgData* battleMsgData,
                                            game::CMidgardID* targetUnitId)
    {
        using namespace game;
        auto& fn = gameFunctions();

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

        return true;
    }

    //HIT
    void __fastcall blisterOnHitHooked(game::CBatAttackBlister* thisptr,
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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        const auto attack = thisptr->attack;

        const bool isItem = idApi.getType(&thisptr->id2) == IdType::Item;

        int damage = attack->vftable->getQtyDamage(attack);

        if (!isItem) {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        int curDamage = targetInfo->blisterAppliedDamage;

        targetInfo->blisterAppliedDamage = std::clamp(curDamage + damage, 0, userSettings().extendedBattle.maxDotDamage);

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Blister, true);

        if (thisptr->attack->vftable->getInfinite(thisptr->attack)) {
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BlisterLong,
                                    true);
        }
        targetInfo->blisterAppliedRound = battleMsgData->currentRound;

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

    void __fastcall frostbiteOnHitHooked(game::CBatAttackFrostbite* thisptr,
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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        const auto attack = thisptr->attack;

        const bool isItem = idApi.getType(&thisptr->id2) == IdType::Item;

        int damage = attack->vftable->getQtyDamage(attack);

        if (!isItem) {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        int curDamage = targetInfo->frostbiteAppliedDamage;

        targetInfo->frostbiteAppliedDamage = std::clamp(curDamage + damage, 0,
                                                        userSettings().extendedBattle.maxDotDamage);

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Frostbite, true);

        if (thisptr->attack->vftable->getInfinite(thisptr->attack)) {
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::FrostbiteLong,
                                    true);
        }
        targetInfo->frostbiteAppliedRound = battleMsgData->currentRound;

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

    void __fastcall poisonOnHitHooked(game::CBatAttackPoison* thisptr,
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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        const auto attack = thisptr->attack;

        const bool isItem = idApi.getType(&thisptr->id2) == IdType::Item;

        int damage = attack->vftable->getQtyDamage(attack);

        if (!isItem)
        {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        int curDamage = targetInfo->poisonAppliedDamage;

        targetInfo->poisonAppliedDamage = std::clamp(curDamage + damage, 0,
                                                     userSettings().extendedBattle.maxDotDamage);

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Poison, true);

        if (thisptr->attack->vftable->getInfinite(thisptr->attack)) {
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::PoisonLong, true);
        }
        targetInfo->poisonAppliedRound = battleMsgData->currentRound;

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        int damage = std::clamp(targetInfo->blisterAppliedDamage, 0,
                                userSettings().extendedBattle.maxDotDamage);

        visitorApi.changeUnitHp(targetUnitId, -damage, objectMap, 1);

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        battleApi.setUnitHp(battleMsgData, targetUnitId, targetUnit->currentHp);

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        info.damage = damage;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        int damage = std::clamp(targetInfo->frostbiteAppliedDamage, 0,
                                userSettings().extendedBattle.maxDotDamage);

        visitorApi.changeUnitHp(targetUnitId, -damage, objectMap, 1);

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        battleApi.setUnitHp(battleMsgData, targetUnitId, targetUnit->currentHp);

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        info.damage = damage;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        int damage = std::clamp(targetInfo->poisonAppliedDamage, 0,
                                userSettings().extendedBattle.maxDotDamage);

        visitorApi.changeUnitHp(targetUnitId, -damage, objectMap, 1);

        battleApi.checkUnitDeath(objectMap, battleMsgData, targetUnitId);

        battleApi.setUnitHp(battleMsgData, targetUnitId, targetUnit->currentHp);

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        info.damage = damage;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

    //Base onHit attacks
    bool __fastcall defaultBlisterCanPerformHooked(game::CBatAttackBlister* thisptr,
                                                   int /*%edx*/,
                                                   game::IMidgardObjectMap* objectMap,
                                                   game::BattleMsgData* battleMsgData,
                                                   game::CMidgardID* targetUnitId)
    {
        using namespace game;
        auto& fn = gameFunctions();
        static const auto& battleApi = BattleMsgDataApi::get();

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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);

        int curDamage = targetInfo->blisterAppliedDamage;
        int attackDamage = thisptr->attack->vftable->getQtyDamage(thisptr->attack);
        int computeDamage = computeDotDamage(&thisptr->unitId, battleMsgData, attackDamage);

        if (curDamage > computeDamage)
            return false;

        return true;
    }

    void __fastcall defaultBlisterOnHitHooked(game::CBatAttackBlister* thisptr,
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

        const auto attack = thisptr->attack;

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BlisterLong, false);
        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Blister, true);

        if (attack->vftable->getInfinite(attack))
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BlisterLong, true);

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        const bool isItem = idApi.getType(&thisptr->id2) == IdType::Item;

        int damage = attack->vftable->getQtyDamage(attack);

        if (!isItem) {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        targetInfo->blisterAppliedDamage = std::clamp(damage, 0,
                                                      userSettings().extendedBattle.maxDotDamage);
        targetInfo->blisterAppliedRound = battleMsgData->currentRound;

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

    bool __fastcall defaultPoisonCanPerformHooked(game::CBatAttackPoison* thisptr,
                                                  int /*%edx*/,
                                                  game::IMidgardObjectMap* objectMap,
                                                  game::BattleMsgData* battleMsgData,
                                                  game::CMidgardID* targetUnitId)
    {
        using namespace game;
        auto& fn = gameFunctions();
        static const auto& battleApi = BattleMsgDataApi::get();

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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);

        int curDamage = targetInfo->poisonAppliedDamage;
        int attackDamage = thisptr->attack->vftable->getQtyDamage(thisptr->attack);
        int computeDamage = computeDotDamage(&thisptr->unitId, battleMsgData, attackDamage);

        if (curDamage > computeDamage)
            return false;

        return true;
    }

    void __fastcall defaultPoisonOnHitHooked(game::CBatAttackPoison* thisptr,
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

        const auto attack = thisptr->attack;

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::PoisonLong, false);
        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Poison, true);

        if (attack->vftable->getInfinite(attack))
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::PoisonLong, true);

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        const bool isItem = idApi.getType(&thisptr->id2) == IdType::Item;

        int damage = attack->vftable->getQtyDamage(attack);

        if (!isItem) {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        targetInfo->poisonAppliedDamage = std::clamp(damage, 0,
                                                      userSettings().extendedBattle.maxDotDamage);
        targetInfo->poisonAppliedRound = battleMsgData->currentRound;

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

    bool __fastcall defaultFrostbiteCanPerformHooked(game::CBatAttackFrostbite* thisptr,
                                                     int /*%edx*/,
                                                     game::IMidgardObjectMap* objectMap,
                                                     game::BattleMsgData* battleMsgData,
                                                     game::CMidgardID* targetUnitId)
    {
        using namespace game;
        auto& fn = gameFunctions();
        static const auto& battleApi = BattleMsgDataApi::get();

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

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);

        int curDamage = targetInfo->frostbiteAppliedDamage;
        int attackDamage = thisptr->attack->vftable->getQtyDamage(thisptr->attack);
        int computeDamage = computeDotDamage(&thisptr->unitId, battleMsgData, attackDamage);

        if (curDamage > computeDamage)
            return false;

        return true;
    }

    void __fastcall defaultFrostbiteOnHitHooked(game::CBatAttackFrostbite* thisptr,
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

        const auto attack = thisptr->attack;

        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::FrostbiteLong, false);
        battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Frostbite, true);

        if (attack->vftable->getInfinite(attack))
            battleApi.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::FrostbiteLong, true);

        UnitInfo* targetInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);
        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);

        const bool isItem = idApi.getType(&thisptr->id2) == IdType::Item;

        int damage = attack->vftable->getQtyDamage(attack);

        if (!isItem) {
            damage = computeDotDamage(&thisptr->unitId, battleMsgData, damage);
        }

        targetInfo->frostbiteAppliedDamage = std::clamp(damage, 0,
                                                        userSettings().extendedBattle.maxDotDamage);
        targetInfo->frostbiteAppliedRound = battleMsgData->currentRound;

        BattleAttackUnitInfo info{};
        info.unitId = targetUnit->id;
        info.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &info);
    }

} // namespace hooks
