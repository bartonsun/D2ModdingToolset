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

#include "attack.h"
#include "batattackblister.h"
#include "batattackfrostbite.h"
#include "batattackpoison.h"
#include "batattackutils.h"
#include "battleattackinfo.h"
#include "dotattackhooks.h"
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

namespace hooks {

    int computeDotDamage(game::CMidgardID* unitId, game::BattleMsgData* battleMsgData, int baseDamage)
    {

        using namespace game;
        auto& battleApi = BattleMsgDataApi::get();

        int boost = 0;
        int lower = 0;

        if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl1))
            boost = 1;
        else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl1))
            boost = 2;
        else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl1))
            boost = 3;
        else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::BoostDamageLvl1))
            boost = 4;

        if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::LowerDamageLvl1))
            lower = 1;
        else if (battleApi.getUnitStatus(battleMsgData, unitId, BattleStatus::LowerDamageLvl2))
            lower = 2;

        int sumBoost = hooks::getBoostDamage(boost) - hooks::getLowerDamage(lower);

        int result = baseDamage + baseDamage * sumBoost / 100;

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

        return true;
    }

    void __fastcall blisterOnHitHooked(game::CBatAttackBlister* thisptr,
                                       int /*%edx*/,
                                       game::IMidgardObjectMap* objectMap,
                                       game::BattleMsgData* battleMsgData,
                                       game::CMidgardID* targetUnitId,
                                       game::BattleAttackInfo** attackInfo)
    {
        using namespace game;
        auto& fn = gameFunctions();

        game::CAttackImpl* attackImpl = getAttackImpl(thisptr->attack);
        int attackImplDamage = attackImpl->data->qtyDamage;

        game::CMidUnit* targetUnit = static_cast<CMidUnit*>(
            objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

        game::UnitInfo* info = BattleMsgDataApi::get().getUnitInfoById(battleMsgData, targetUnitId);

        auto attack = bindings::IdView{userSettings().extendedBattle.blisterDamageID};

        int damage = computeDotDamage(&thisptr->unitId, battleMsgData, attackImplDamage);

        if (damage > 0) {

            bool infinite = thisptr->attack->vftable->getInfinite(thisptr->attack);

            auto hasBlister = info->blisterAttackId;

            if (hasBlister == game::emptyId) {
                info->blisterAttackId = attack.id;
                info->dotInfo.blisterDamage = std::clamp(damage, 0, userSettings().extendedBattle.maxDotDamage);
            } else {
                int curDamage = info->dotInfo.blisterDamage;
                damage += curDamage;

                info->blisterAttackId = attack.id;
                info->dotInfo.blisterDamage = std::clamp(damage, 0, userSettings().extendedBattle.maxDotDamage);
            }

            BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Blister, true);

            // Update Long status, replace short status
            if (infinite) {
                BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId,
                                                      BattleStatus::BlisterLong, infinite);
                info->blisterAppliedRound = battleMsgData->currentRound;
            }
        }

        BattleAttackUnitInfo attInfo{};
        attInfo.unitId = targetUnit->id;
        attInfo.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &attInfo);
    }

    void __fastcall frostbiteOnHitHooked(game::CBatAttackFrostbite* thisptr,
                                       int /*%edx*/,
                                       game::IMidgardObjectMap* objectMap,
                                       game::BattleMsgData* battleMsgData,
                                       game::CMidgardID* targetUnitId,
                                       game::BattleAttackInfo** attackInfo)
    {
        using namespace game;
        auto& fn = gameFunctions();

        game::CAttackImpl* attackImpl = getAttackImpl(thisptr->attack);
        int attackImplDamage = attackImpl->data->qtyDamage;

        game::CMidUnit* targetUnit = static_cast<CMidUnit*>(
            objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

        game::UnitInfo* info = BattleMsgDataApi::get().getUnitInfoById(battleMsgData, targetUnitId);

        auto attack = bindings::IdView{userSettings().extendedBattle.frostbiteDamageID};

        int damage = computeDotDamage(&thisptr->unitId, battleMsgData, attackImplDamage);

        if (damage > 0) {

            bool infinite = thisptr->attack->vftable->getInfinite(thisptr->attack);

            auto hasFrostbite = info->frostbiteAttackId;

            if (hasFrostbite == game::emptyId) {
                info->frostbiteAttackId = attack.id;
                info->dotInfo.frostbiteDamage = std::clamp(damage, 0, userSettings().extendedBattle.maxDotDamage);
            } else {
                int curDamage = info->dotInfo.frostbiteDamage;
                damage += curDamage;

                info->frostbiteAttackId = attack.id;
                info->dotInfo.frostbiteDamage = std::clamp(damage, 0, userSettings().extendedBattle.maxDotDamage);
            }

            BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Frostbite, true);

            // Update Long status, replace short status
            if (infinite) {
                BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId,
                                                      BattleStatus::FrostbiteLong, infinite);
                info->frostbiteAppliedRound = battleMsgData->currentRound;
            }
        }

        BattleAttackUnitInfo attInfo{};
        attInfo.unitId = targetUnit->id;
        attInfo.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &attInfo);
    }

    void __fastcall poisonOnHitHooked(game::CBatAttackPoison* thisptr,
                                      int /*%edx*/,
                                      game::IMidgardObjectMap* objectMap,
                                      game::BattleMsgData* battleMsgData,
                                      game::CMidgardID* targetUnitId,
                                      game::BattleAttackInfo** attackInfo)
    {
        using namespace game;
        auto& fn = gameFunctions();

        game::CAttackImpl* attackImpl = getAttackImpl(thisptr->attack);
        int attackImplDamage = attackImpl->data->qtyDamage;

        game::CMidUnit* targetUnit = static_cast<CMidUnit*>(objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

        game::UnitInfo* info = BattleMsgDataApi::get().getUnitInfoById(battleMsgData, targetUnitId);

        auto attack = bindings::IdView{userSettings().extendedBattle.poisonDamageID};

        int damage = computeDotDamage(&thisptr->unitId, battleMsgData, attackImplDamage);

        if (damage > 0) {

            bool infinite = thisptr->attack->vftable->getInfinite(thisptr->attack);

            auto hasPoison = info->poisonAttackId;

            if (hasPoison == game::emptyId) {
                info->poisonAttackId = attack.id;
                info->dotInfo.poisonDamage = std::clamp(damage, 0, userSettings().extendedBattle.maxDotDamage);
            } 
            else
            {
                int curDamage = info->dotInfo.poisonDamage;
                damage += curDamage;

                info->poisonAttackId = attack.id;
                info->dotInfo.poisonDamage = std::clamp(damage, 0, userSettings().extendedBattle.maxDotDamage);
            }

            BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Poison, true);

            // Update Long status, replace short status
            if (infinite) {
                BattleMsgDataApi::get().setUnitStatus(battleMsgData, targetUnitId,
                                                      BattleStatus::PoisonLong, infinite);
                info->poisonAppliedRound = battleMsgData->currentRound;
            }
        }

        BattleAttackUnitInfo attInfo{};
        attInfo.unitId = targetUnit->id;
        attInfo.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &attInfo);
    }
} // namespace hooks