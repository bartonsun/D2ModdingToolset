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

#include "batattackutils.h"
#include "batattacktransformself.h"
#include "batattackuseorb.h"
#include "batattackusetalisman.h"
#include "dynamiccast.h"
#include "visitors.h"

#include <attackutils.h>
#include <restrictions.h>
#include <sol/sol.hpp>

namespace hooks {

int computeBoostedHeal(game::CMidgardID* unitId, game::BattleMsgData* battleMsgData, int baseHeal)
{
    using namespace game;
    auto& battleApi = BattleMsgDataApi::get();
    const auto& restrictions = gameRestrictions();

    int boost = 0;
    int lower = 0;
    double heal = baseHeal;

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

    int result = int( heal + heal * sumBoost / 100 );

    return std::clamp(result, restrictions.attackHeal.min, restrictions.attackHeal.max);
}

bool canHeal(game::IAttack* attack,
             game::IMidgardObjectMap* objectMap,
             game::BattleMsgData* battleMsgData,
             game::CMidgardID* targetUnitId)
{
    using namespace game;

    const auto qtyHeal = attack->vftable->getQtyHeal(attack);
    if (qtyHeal <= 0)
        return false;

    return BattleMsgDataApi::get().unitCanBeHealed(objectMap, battleMsgData, targetUnitId);
}

int heal(game::IMidgardObjectMap* objectMap,
         game::BattleMsgData* battleMsgData,
         game::CMidUnit* targetUnit,
         int qtyHeal)
{
    using namespace game;

    int hpBefore = targetUnit->currentHp;

    const auto& visitors = VisitorApi::get();
    visitors.changeUnitHp(&targetUnit->id, qtyHeal, objectMap, 1);

    int hpAfter = targetUnit->currentHp;
    BattleMsgDataApi::get().setUnitHp(battleMsgData, &targetUnit->id, hpAfter);

    return hpAfter - hpBefore;
}

const game::CMidgardID* getUnitId(const game::IBatAttack* batAttack)
{
    using namespace game;

    const auto& rtti = RttiApi::rtti();
    const auto dynamicCast = RttiApi::get().dynamicCast;

    auto transformSelfAttack = (CBatAttackTransformSelf*)
        dynamicCast(batAttack, 0, rtti.IBatAttackType, rtti.CBatAttackTransformSelfType, 0);
    if (transformSelfAttack) {
        return &transformSelfAttack->unitId;
    }

    auto useOrbAttack = (CBatAttackUseOrb*)dynamicCast(batAttack, 0, rtti.IBatAttackType,
                                                       rtti.CBatAttackUseOrbType, 0);
    if (useOrbAttack) {
        return &useOrbAttack->unitId;
    }

    auto useTalismanAttack = (CBatAttackUseTalisman*)dynamicCast(batAttack, 0, rtti.IBatAttackType,
                                                                 rtti.CBatAttackUseTalismanType, 0);
    if (useTalismanAttack) {
        return &useTalismanAttack->unitId;
    }

    // HACK: every other attack in the game has its unitId as a first field, but its not a part
    // of CBatAttackBase.
    return (CMidgardID*)(batAttack + 1);
}

const game::CMidgardID* getItemId(const game::IBatAttack* batAttack)
{
    using namespace game;

    const auto& rtti = RttiApi::rtti();
    const auto dynamicCast = RttiApi::get().dynamicCast;

    auto useOrbAttack = (CBatAttackUseOrb*)dynamicCast(batAttack, 0, rtti.IBatAttackType,
                                                       rtti.CBatAttackUseOrbType, 0);
    if (useOrbAttack) {
        return &useOrbAttack->itemId;
    }

    auto useTalismanAttack = (CBatAttackUseTalisman*)dynamicCast(batAttack, 0, rtti.IBatAttackType,
                                                                 rtti.CBatAttackUseTalismanType, 0);
    if (useTalismanAttack) {
        return &useTalismanAttack->itemId;
    }

    return &emptyId;
}

} // namespace hooks
