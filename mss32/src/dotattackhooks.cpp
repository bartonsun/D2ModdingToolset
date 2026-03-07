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
        double damage = baseDamage;

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
        auto& battle = BattleMsgDataApi::get();

        game::UnitInfo* info = battle.getUnitInfoById(battleMsgData, targetUnitId);
        game::CMidUnit* targetUnit = static_cast<CMidUnit*>(objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

        game::CAttackImpl* attackImpl = getAttackImpl(thisptr->attack);

        int attackNumber = thisptr->attackNumber;
        int damage = (attackNumber == 1) ? computeDotDamage(&thisptr->unitId, battleMsgData,
                                                            attackImpl->data->qtyDamage)
                                                : attackImpl->data->qtyDamage;

        if (damage > 0) {
            int curDamage = (info->blisterAttackId == game::emptyId) ? 0 : info->dotInfo.blisterDamage;

            info->blisterAttackId = bindings::IdView{userSettings().extendedBattle.blisterDamageID};
            info->dotInfo.blisterDamage = std::clamp(curDamage + damage, 0, userSettings().extendedBattle.maxDotDamage);

            battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Blister, true);

            if (thisptr->attack->vftable->getInfinite(thisptr->attack)) {
                battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::BlisterLong, true);
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
        auto& battle = BattleMsgDataApi::get();

        game::UnitInfo* info = battle.getUnitInfoById(battleMsgData, targetUnitId);
        game::CMidUnit* targetUnit = static_cast<CMidUnit*>(
            objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

        game::CAttackImpl* attackImpl = getAttackImpl(thisptr->attack);

        int attackNumber = thisptr->attackNumber;
        int damage = (attackNumber == 1) ? computeDotDamage(&thisptr->unitId, battleMsgData,
                                                            attackImpl->data->qtyDamage)
                                         : attackImpl->data->qtyDamage;

        if (damage > 0) {
            int curDamage = (info->frostbiteAttackId == game::emptyId)
                                ? 0
                                : info->dotInfo.frostbiteDamage;

            info->frostbiteAttackId = bindings::IdView{
                userSettings().extendedBattle.frostbiteDamageID};
            info->dotInfo.frostbiteDamage = std::clamp(curDamage + damage, 0,
                                                     userSettings().extendedBattle.maxDotDamage);

            battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Frostbite, true);

            if (thisptr->attack->vftable->getInfinite(thisptr->attack)) {
                battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::FrostbiteLong, true);
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
        auto& battle = BattleMsgDataApi::get();

        game::UnitInfo* info = battle.getUnitInfoById(battleMsgData, targetUnitId);
        game::CMidUnit* targetUnit = static_cast<CMidUnit*>(
            objectMap->vftable->findScenarioObjectByIdForChange(objectMap, targetUnitId));

        game::CAttackImpl* attackImpl = getAttackImpl(thisptr->attack);

        int attackNumber = thisptr->attackNumber;
        int damage = (attackNumber == 1) ? computeDotDamage(&thisptr->unitId, battleMsgData,
                                                            attackImpl->data->qtyDamage)
                                         : attackImpl->data->qtyDamage;

        if (damage > 0) {
            int curDamage = (info->poisonAttackId == game::emptyId) ? 0
                                                                      : info->dotInfo.poisonDamage;

            info->poisonAttackId = bindings::IdView{userSettings().extendedBattle.poisonDamageID};
            info->dotInfo.poisonDamage = std::clamp(curDamage + damage, 0,
                                                     userSettings().extendedBattle.maxDotDamage);

            battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::Poison, true);

            if (thisptr->attack->vftable->getInfinite(thisptr->attack)) {
                battle.setUnitStatus(battleMsgData, targetUnitId, BattleStatus::PoisonLong, true);
                info->poisonAppliedRound = battleMsgData->currentRound;
            }
        }

        BattleAttackUnitInfo attInfo{};
        attInfo.unitId = targetUnit->id;
        attInfo.unitImplId = targetUnit->unitImpl->id;
        BattleAttackInfoApi::get().addUnitInfo(&(*attackInfo)->unitsInfo, &attInfo);
    }
} // namespace hooks