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

#include "reviveattackhooks.h"
#include "attack.h"
#include "batattackrevive.h"
#include "battleattackinfo.h"
#include "battlemsgdata.h"
#include "game.h"
#include "immunecat.h"
#include "midunit.h"
#include "settings.h"
#include "ussoldier.h"
#include "usunit.h"
#include "unitutils.h"
#include "batattackutils.h"
#include "visitors.h"
#include <attackparams.h>
#include <hooks.h>

namespace hooks { 

	bool __fastcall reviveAttackIsImmuneHooked(game::CBatAttackRevive* thisptr,
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

        IAttack* attack = fn.getAttackById(objectMap, &thisptr->attackImplUnitId, thisptr->attackNumber,
                                           false);

        if (!attack) {
            return false;
        }

        return IsImmuneToAttack(battleMsgData, objectMap, unitId, attack);
    }

    void __fastcall reviveAttackOnHitHooked(game::CBatAttackRevive* thisptr,
                                            int /*%edx*/,
                                            game::IMidgardObjectMap* objectMap,
                                            game::BattleMsgData* battleMsgData,
                                            game::CMidgardID* targetUnitId,
                                            game::BattleAttackInfo** attackInfo)
    {
        using namespace game;

        static const auto& idApi = CMidgardIDApi::get();
        static const auto& battleApi = BattleMsgDataApi::get();
        static const auto& visitor = VisitorApi::get();
        static const auto& fn = gameFunctions();
        static const auto& globalApi = GlobalDataApi::get();
        auto* globalData = *globalApi.getGlobalData();

        const auto& settings = gameSettings();

        CMidUnit* targetUnit = fn.findUnitById(objectMap, targetUnitId);
        if (!targetUnit)
            return;

        UnitInfo* targetUnitInfo = battleApi.getUnitInfoById(battleMsgData, targetUnitId);

        visitor.reviveUnit(&targetUnit->id, objectMap, 1);
        targetUnitInfo->unitFlags.parts.revived = true;

        int qtyHealed = 1;
        const bool isItem = idApi.getType(&thisptr->attackImplUnitId) == IdType::Item;

        if (!isItem && settings.reviveAttacksUsesQtyHeal == 1) {
            const CMidUnit* reviveUnit = fn.findUnitById(objectMap, &thisptr->unitId);
            const IAttack* attack = getAttack(reviveUnit->unitImpl, true, false);
            const int qtyHeal = attack->vftable->getQtyHeal(attack);
            if (qtyHeal)
                qtyHealed = computeBoostedHeal(&thisptr->unitId, battleMsgData, qtyHeal);
        } else {

            const IAttack* reviveAttack = thisptr->attackImpl;
            if (!isItem && settings.reviveAttacksUsesQtyHeal == 0)
                reviveAttack = (IAttack*)globalApi.findById(globalData->attacks,
                                                            &thisptr->attackImpl->id);

            const int reviveQtyHeal = reviveAttack->vftable->getQtyHeal(reviveAttack);
            const IUsSoldier* soldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);
            const int maxHp = soldier->vftable->getHitPoints(soldier);
            int targetHp = reviveQtyHeal * maxHp / 100;

            if (!isItem && settings.reviveAttacksUsesQtyHeal == 2)
                targetHp = computeBoostedHeal(&thisptr->unitId, battleMsgData, reviveQtyHeal);

            if (isItem && settings.reviveItemsUsesQtyHeal)
                targetHp = reviveQtyHeal;

            qtyHealed = targetHp;
        }

        CMidUnit* unitAttacker = game::gameFunctions().findUnitById(objectMap, &thisptr->unitId);

        bindings::AttackHitParamsView params;
        params.attacker = unitAttacker;
        params.target = targetUnit;
        params.attack = thisptr->attackImpl;
        params.attackClass = "Revive";
        params.heal = qtyHealed;
        params.miss = false;

        callLuaAttackHook(objectMap, battleMsgData, params);

        if (params.miss) {
            addToBattleAttackInfo(*attackInfo, targetUnit, 0, 0, true);
            return;
        }

        qtyHealed = params.heal;

        visitor.changeUnitHp(targetUnitId, qtyHealed - 1, objectMap, 1);

        battleApi.setUnitStatus(battleMsgData, &targetUnit->id, BattleStatus::Dead, false);
        battleApi.setUnitStatus(battleMsgData, &targetUnit->id, BattleStatus::XpCounted, false);

        battleApi.setUnitHp(battleMsgData, targetUnitId, targetUnit->currentHp);

        addToBattleAttackInfo(*attackInfo, targetUnit);
    }

} // namespace hooks
