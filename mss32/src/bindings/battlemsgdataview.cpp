/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Stanislav Egorov.
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

#include "battlemsgdataview.h"
#include "attackclasscat.h"
#include "attackutils.h"
#include "battlemsgdata.h"
#include "customattacks.h"
#include "game.h"
#include "gameutils.h"
#include "idview.h"
#include "playerview.h"
#include "stackview.h"
#include <sol/sol.hpp>
#include <modifierutils.h>
#include <settings.h>
#include <visitors.h>
#include <midunit.h>
#include <ussoldier.h>

namespace bindings {

BattleTurnView::BattleTurnView(const game::CMidgardID& unitId,
                               char attackCount,
                               const game::IMidgardObjectMap* objectMap)
    : unitId{unitId}
    , objectMap{objectMap}
    , attackCount{attackCount}
{ }

void BattleTurnView::bind(sol::state& lua)
{
    auto view = lua.new_usertype<BattleTurnView>("BattleTurnView");
    view["unit"] = sol::property(&BattleTurnView::getUnit);
    view["attackCount"] = sol::property(&BattleTurnView::getAttackCount);
}

UnitView BattleTurnView::getUnit() const
{
    return UnitView{game::gameFunctions().findUnitById(objectMap, &unitId)};
}

int BattleTurnView::getAttackCount() const
{
    return attackCount;
}

BattleMsgDataView::BattleMsgDataView(const game::BattleMsgData* battleMsgData,
                                     const game::IMidgardObjectMap* objectMap)
    : battleMsgData{battleMsgData}
    , objectMap{objectMap}
{ }

void BattleMsgDataView::bind(sol::state& lua)
{
    auto view = lua.new_usertype<BattleMsgDataView>("BattleView");
    bindAccessMethods(view);
}

bool BattleMsgDataView::getUnitStatus(const IdView& unitId, int status) const
{
    using namespace game;

    return BattleMsgDataApi::get().getUnitStatus(battleMsgData, &unitId.id, (BattleStatus)status);
}

int BattleMsgDataView::getCurrentRound() const
{
    return battleMsgData->currentRound;
}

bool BattleMsgDataView::getAutoBattle() const
{
    return game::BattleMsgDataApi::get().isAutoBattle(battleMsgData);
}

bool BattleMsgDataView::getFastBattle() const
{
    return game::BattleMsgDataApi::get().isFastBattle(battleMsgData);
}

std::optional<PlayerView> BattleMsgDataView::getAttackerPlayer() const
{
    return getPlayer(battleMsgData->attackerPlayerId);
}

std::optional<PlayerView> BattleMsgDataView::getDefenderPlayer() const
{
    return getPlayer(battleMsgData->defenderPlayerId);
}

std::optional<StackView> BattleMsgDataView::getAttacker() const
{
    auto stack{hooks::getStack(objectMap, &battleMsgData->attackerGroupId)};
    if (!stack) {
        return std::nullopt;
    }

    return StackView{stack, objectMap};
}

std::optional<GroupView> BattleMsgDataView::getDefender() const
{
    auto group{hooks::getGroup(objectMap, &battleMsgData->defenderGroupId)};
    if (!group) {
        return std::nullopt;
    }

    return GroupView{group, objectMap, &battleMsgData->defenderGroupId};
}

game::RetreatStatus BattleMsgDataView::getRetreatStatus(bool attacker) const
{
    return game::BattleMsgDataApi::get().getRetreatStatus(battleMsgData, attacker);
}

bool BattleMsgDataView::isRetreatDecisionWasMade() const
{
    return game::BattleMsgDataApi::get().isRetreatDecisionWasMade(battleMsgData);
}

bool BattleMsgDataView::isUnitAttacker(const UnitView& unit) const
{
    return isUnitAttackerId(unit.getId());
}

bool BattleMsgDataView::isUnitAttackerId(const IdView& unitId) const
{
    return game::BattleMsgDataApi::get().isUnitAttacker(battleMsgData, &unitId.id);
}

bool BattleMsgDataView::isAfterBattle() const
{
    return game::BattleMsgDataApi::get().isAfterBattle(battleMsgData);
}

bool BattleMsgDataView::isDuel() const
{
    return battleMsgData->duel & 1u;
}

BattleMsgDataView::UnitActions BattleMsgDataView::getUnitActions(const UnitView& unit) const
{
    return getUnitActionsById(unit.getId());
}

BattleMsgDataView::UnitActions BattleMsgDataView::getUnitActionsById(const IdView& unitId) const
{
    using namespace game;

    std::vector<BattleAction> actions;
    std::optional<GroupView> attackTargetGroup;
    std::vector<int> attackTargets;
    std::optional<GroupView> item1TargetGroup;
    std::vector<int> item1Targets;
    std::optional<GroupView> item2TargetGroup;
    std::vector<int> item2Targets;

    {
        const auto& setApi = IntSetApi::get();

        Set<BattleAction> battleActions;
        setApi.constructor((IntSet*)&battleActions);

        GroupIdTargetsPair attackTargetsPair{};
        setApi.constructor(&attackTargetsPair.second);

        GroupIdTargetsPair item1TargetsPair{};
        setApi.constructor(&item1TargetsPair.second);

        GroupIdTargetsPair item2TargetsPair{};
        setApi.constructor(&item2TargetsPair.second);

        game::BattleMsgDataApi::get().updateBattleActions(objectMap, battleMsgData, &unitId.id,
                                                          &battleActions, &attackTargetsPair,
                                                          &item1TargetsPair, &item2TargetsPair);

        // Copy data into std containers for sol2
        {
            actions.reserve(battleActions.length);
            for (auto& v : battleActions) {
                actions.push_back(v);
            }

            if (const auto* group = hooks::getGroup(objectMap, &attackTargetsPair.first)) {
                attackTargetGroup = bindings::GroupView{group, objectMap, &attackTargetsPair.first};
            }

            attackTargets.reserve(attackTargetsPair.second.length);
            for (auto& v : attackTargetsPair.second) {
                attackTargets.push_back(v);
            }

            if (const auto* group = hooks::getGroup(objectMap, &item1TargetsPair.first)) {
                item1TargetGroup = bindings::GroupView{group, objectMap, &item1TargetsPair.first};
            }

            item1Targets.reserve(item1TargetsPair.second.length);
            for (auto& v : item1TargetsPair.second) {
                item1Targets.push_back(v);
            }

            if (const auto* group = hooks::getGroup(objectMap, &item2TargetsPair.first)) {
                item2TargetGroup = bindings::GroupView{group, objectMap, &item2TargetsPair.first};
            }

            item2Targets.reserve(item2TargetsPair.second.length);
            for (auto& v : item2TargetsPair.second) {
                item2Targets.push_back(v);
            }
        }

        setApi.destructor(&item2TargetsPair.second);
        setApi.destructor(&item1TargetsPair.second);
        setApi.destructor(&attackTargetsPair.second);
        setApi.destructor((IntSet*)&battleActions);
    }

    return {actions,      attackTargetGroup, attackTargets, item1TargetGroup,
            item1Targets, item2TargetGroup,  item2Targets};
}

int BattleMsgDataView::getUnitShatteredArmor(const UnitView& unit) const
{
    return getUnitShatteredArmorById(unit.getId());
}

int BattleMsgDataView::getUnitShatteredArmorById(const IdView& unitId) const
{
    return game::BattleMsgDataApi::get().getUnitShatteredArmor(battleMsgData, &unitId.id);
}

int BattleMsgDataView::getUnitFortificationArmor(const UnitView& unit) const
{
    return getUnitFortificationArmorById(unit.getId());
}

int BattleMsgDataView::getUnitFortificationArmorById(const IdView& unitId) const
{
    return game::BattleMsgDataApi::get().getUnitFortificationArmor(battleMsgData, &unitId.id);
}

bool BattleMsgDataView::isUnitResistantToSource(const UnitView& unit, int sourceId) const
{
    return isUnitResistantToSourceById(unit.getId(), sourceId);
}

bool BattleMsgDataView::isUnitResistantToSourceById(const IdView& unitId, int sourceId) const
{
    using namespace game;

    auto& fn = gameFunctions();

    auto attackSource{hooks::getAttackSourceById(static_cast<game::AttackSourceId>(sourceId))};
    if (!attackSource) {
        return false;
    }

    const CMidUnit* targetUnit = fn.findUnitById(objectMap, &unitId.id);
    const IUsSoldier* targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);
    const LImmuneCat* immuneCat = targetSoldier->vftable->getImmuneByAttackSource(targetSoldier,
                                                                                  attackSource);

    if (immuneCat->id == ImmuneCategories::get().always->id)
        return true;

    return !BattleMsgDataApi::get().isUnitAttackSourceWardRemoved(battleMsgData, &unitId.id,
                                                                  attackSource);
}

bool BattleMsgDataView::isUnitResistantToClass(const UnitView& unit, int classId) const
{
    return isUnitResistantToClassById(unit.getId(), classId);
}

bool BattleMsgDataView::isUnitResistantToClassById(const IdView& unitId, int classId) const
{
    using namespace game;

    auto& fn = gameFunctions();

    auto attackClass{hooks::getAttackClassById(static_cast<AttackClassId>(classId))};
    if (!attackClass) {
        return false;
    }

    const CMidUnit* targetUnit = fn.findUnitById(objectMap, &unitId.id);
    const IUsSoldier* targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);
    const LImmuneCat* immuneCat = targetSoldier->vftable->getImmuneByAttackClass(targetSoldier,
                                                                                 attackClass);

    if (immuneCat->id == ImmuneCategories::get().always->id)
        return true;

    return BattleMsgDataApi::get().isUnitAttackClassWardRemoved(battleMsgData, &unitId.id,
                                                                attackClass);
}

std::vector<BattleTurnView> BattleMsgDataView::getTurnsOrder() const
{
    std::vector<BattleTurnView> turns;

    for (const auto& battleTurn : battleMsgData->turnsOrder) {
        if (battleTurn.unitId == game::invalidId) {
            break;
        }

        turns.emplace_back(battleTurn.unitId, battleTurn.attackCount, objectMap);
    }

    return turns;
}

bool BattleMsgDataView::isUnitRevived(const UnitView& unit) const
{
    return isUnitRevivedById(unit.getId());
}

bool BattleMsgDataView::isUnitRevivedById(const IdView& unitId) const
{
    const auto* info{game::BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id)};
    if (!info) {
        return false;
    }

    return info->unitFlags.parts.revived;
}

bool BattleMsgDataView::isUnitWaiting(const UnitView& unit) const
{
    return isUnitWaitingById(unit.getId());
}

bool BattleMsgDataView::isUnitWaitingById(const IdView& unitId) const
{
    const auto* info{game::BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id)};
    if (!info) {
        return false;
    }

    return info->unitFlags.parts.waited;
}

int BattleMsgDataView::getUnitDisableRound(const UnitView& unit) const
{
    return getUnitDisableRoundById(unit.getId());
}

int BattleMsgDataView::getUnitDisableRoundById(const IdView& unitId) const
{
    const auto* info{game::BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id)};
    if (!info) {
        return 0;
    }

    return info->disableAppliedRound;
}

int BattleMsgDataView::getUnitPoisonRound(const UnitView& unit) const
{
    return getUnitPoisonRoundById(unit.getId());
}

int BattleMsgDataView::getUnitPoisonRoundById(const IdView& unitId) const
{
    const auto* info{game::BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id)};
    if (!info) {
        return 0;
    }

    return info->poisonAppliedRound;
}

int BattleMsgDataView::getUnitFrostbiteRound(const UnitView& unit) const
{
    return getUnitFrostbiteRoundById(unit.getId());
}

int BattleMsgDataView::getUnitFrostbiteRoundById(const IdView& unitId) const
{
    const auto* info{game::BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id)};
    if (!info) {
        return 0;
    }

    return info->frostbiteAppliedRound;
}

int BattleMsgDataView::getUnitBlisterRound(const UnitView& unit) const
{
    return getUnitBlisterRoundById(unit.getId());
}

int BattleMsgDataView::getUnitBlisterRoundById(const IdView& unitId) const
{
    const auto* info{game::BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id)};
    if (!info) {
        return 0;
    }

    return info->blisterAppliedRound;
}

int BattleMsgDataView::getUnitTransformRound(const UnitView& unit) const
{
    return getUnitTransformRoundById(unit.getId());
}

int BattleMsgDataView::getUnitTransformRoundById(const IdView& unitId) const
{
    const auto* info{game::BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id)};
    if (!info) {
        return 0;
    }

    return info->transformAppliedRound;
}

std::optional<PlayerView> BattleMsgDataView::getPlayer(const game::CMidgardID& playerId) const
{
    auto player{hooks::getPlayer(objectMap, &playerId)};
    if (!player) {
        return std::nullopt;
    }

    return PlayerView{player, objectMap};
}

int BattleMsgDataView::getUnitAttackCount(const IdView& unitId) const
{
    int attackCount = 0;
    for (auto& turns : battleMsgData->turnsOrder) {
        if (turns.unitId == unitId.id) {
            attackCount = turns.attackCount;
            break;
        }
    }

    return attackCount;
}

UnitView BattleMsgDataView::getUnitTurn() const
{   
    auto& turns = battleMsgData->turnsOrder;
    auto unitId = turns[0].unitId;
    return UnitView{game::gameFunctions().findUnitById(objectMap, &unitId)};
}

bool BattleMsgDataView::setUnitAttackCount(const IdView& unitId, int value)
{
    using namespace game;

    auto battle = const_cast<game::BattleMsgData*>(battleMsgData);

    for (auto& turn : battle->turnsOrder) {
        if (turn.unitId == unitId.id) {
            if (value < 1) {
                while (BattleMsgDataApi::get().decreaseUnitAttacks(battle, &unitId.id))
                    ;
                return true;
            }
            turn.attackCount = value;
            return true;
        }
    }

    return false;
}

bool BattleMsgDataView::addUnitModifier(const IdView& unitId2, const IdView& unitId, const std::string& modifierId)
{
    using namespace game;
    const auto& fn = gameFunctions();

    auto battle = const_cast<game::BattleMsgData*>(battleMsgData);

    IMidgardObjectMap* objectMap = const_cast<game::IMidgardObjectMap*>(hooks::getObjectMap());
    CMidUnit* targetUnit = fn.findUnitById(objectMap, &unitId2.id);

    const auto& modId = IdView{modifierId};

    if (hooks::canApplyModifier(battle, targetUnit, &modId.id))
        hooks::applyModifier(&unitId.id, battle, targetUnit, &modId.id);

    return true;
}

int BattleMsgDataView::heal(const IdView& unitId, int value)
{

    using namespace game;
    const auto& fn = gameFunctions();
    const auto& visitors = VisitorApi::get();

    if (BattleMsgDataApi::get().getUnitStatus(battleMsgData, &unitId.id, BattleStatus::Dead))
        return 0;

    int heal = std::clamp(value, -9999, 9999);

    BattleMsgData* battle = const_cast<game::BattleMsgData*>(battleMsgData);

    IMidgardObjectMap* objectMap = const_cast<game::IMidgardObjectMap*>(hooks::getObjectMap());
    CMidUnit* targetUnit = fn.findUnitById(objectMap, &unitId.id);

    int hpBefore = targetUnit->currentHp;

    visitors.changeUnitHp(&targetUnit->id, heal, objectMap, 1);

    int hpAfter = targetUnit->currentHp;
    BattleMsgDataApi::get().setUnitHp(battle, &targetUnit->id, hpAfter);

    int qtyHealed = hpAfter - hpBefore;

    if (targetUnit->currentHp == 0) 
    {
        //BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id, BattleStatus::Dead, true);
        //BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id, BattleStatus::XpCounted, true);
        //First globalmap, then battle
        targetUnit->currentHp = 1;
        BattleMsgDataApi::get().setUnitHp(battle, &targetUnit->id, 1);
        qtyHealed++;
    }

    return qtyHealed;
}

int BattleMsgDataView::setHealth(const IdView& unitId, int value)
{
    using namespace game;
    const auto& fn = gameFunctions();

    if (BattleMsgDataApi::get().getUnitStatus(battleMsgData, &unitId.id, BattleStatus::Dead))
        return 0;

    auto objectMap = const_cast<game::IMidgardObjectMap*>(hooks::getObjectMap());
    CMidUnit* targetUnit = fn.findUnitById(objectMap, &unitId.id);
    auto battle = const_cast<game::BattleMsgData*>(battleMsgData);

    int health = std::clamp(value, 1, 9999);

    const auto& visitors = VisitorApi::get();
    visitors.changeUnitHp(&unitId.id, health-targetUnit->currentHp, objectMap, 1);

    BattleMsgDataApi::get().setUnitHp(battle, &unitId.id, health);

    return targetUnit->currentHp;
}

bool BattleMsgDataView::setShatteredArmor(const IdView& unitId, int value)
{
    using namespace game;

    auto info = BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id);
    if (!info) {
        return false;
    }

    int maxShatter = hooks::userSettings().shatteredArmorMax;

    info->shatteredArmor = std::clamp(info->shatteredArmor + value, 0, maxShatter);

    return true;
}

int BattleMsgDataView::getStatusDamage(const IdView& unitId, const int status)
{
    using namespace game;
    
    auto info = BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id);
    if (!info) {
        return false;
    }

    switch (BattleStatus(status))
    {
        case BattleStatus::Poison:
        {
            CMidgardID poisonId = info->poisonAttackId;

            if (poisonId == game::emptyId)
                return 0;

            std::string poisonIDstr = hooks::idToString(&poisonId, true);
            std::string damage = poisonIDstr.substr(6);
            int cur = std::stoi(damage);

            return cur;
        }
        case BattleStatus::Frostbite: {
            CMidgardID frostbiteId = info->frostbiteAttackId;

            if (frostbiteId == game::emptyId)
                return 0;

            std::string frostbiteIDstr = hooks::idToString(&frostbiteId, true);
            std::string damage = frostbiteIDstr.substr(6);
            int cur = std::stoi(damage);

            return cur;
        }
        case BattleStatus::Blister: {
            CMidgardID blisterId = info->blisterAttackId;

            if (blisterId == game::emptyId)
                return 0;

            std::string blisterIDstr = hooks::idToString(&blisterId, true);
            std::string damage = blisterIDstr.substr(6);
            int cur = std::stoi(damage);

            return cur;
        }
    }

    return 0;
}

bool BattleMsgDataView::setStatus(const IdView& unitId, int status, int value, bool isLong)
{
    using namespace game;

    auto info = BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id);
    if (!info) {
        return false;
    }

    std::string dmg = std::to_string(std::clamp(value, 1, hooks::userSettings().extendedBattle.maxDotDamage));
    dmg.insert(0, 4 - dmg.length(), '0');

    switch (BattleStatus(status))
    {
        case BattleStatus::Poison:
        {
        auto attack = IdView{hooks::userSettings().extendedBattle.poisonDamageID + dmg};
            info->poisonAttackId = attack.id;
            info->poisonAppliedRound = battleMsgData->currentRound;

            BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id, BattleStatus::Poison,
                                                  true);
            BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id, BattleStatus::PoisonLong,
                                                  isLong);
            return true;
        }
        case BattleStatus::Frostbite:
        {
            auto attack = IdView{hooks::userSettings().extendedBattle.frostbiteDamageID + dmg};
            info->frostbiteAttackId = attack.id;
            info->frostbiteAppliedRound = battleMsgData->currentRound;

            BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id,
                                                  BattleStatus::Frostbite, true);
            BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id,
                                                  BattleStatus::FrostbiteLong, isLong);

            return true;
        }
        case BattleStatus::Blister:
        {
            auto attack = IdView{hooks::userSettings().extendedBattle.blisterDamageID + dmg};
            info->blisterAttackId = attack.id;
            info->blisterAppliedRound = battleMsgData->currentRound;

            BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id, BattleStatus::Blister,
                                                  true);
            BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id,
                                                  BattleStatus::BlisterLong, isLong);

            return true;
        }
        case BattleStatus::Paralyze:
        {
            BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id, BattleStatus::Paralyze,
                                                  true);
            return true;
        }
        case BattleStatus::Petrify:
        {
            BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id, BattleStatus::Petrify,
                                                  true);
            return true;
        }
        default:
        {
            return false;
        }
    }
}

bool BattleMsgDataView::cure(const IdView& unitId)
{
    using namespace game;

    BattleMsgDataApi::get().setUnitStatus(battleMsgData, &unitId.id, BattleStatus::Cured, true);

    return true;
}

void BattleMsgDataView::removeAttackSourceWard(const IdView& unitId, int attackSourceId)
{
    using namespace game;
    auto& fn = gameFunctions();

    auto objectMap = const_cast<game::IMidgardObjectMap*>(hooks::getObjectMap());
    auto battle = const_cast<game::BattleMsgData*>(battleMsgData);

    const AttackSourceId srcId{static_cast<AttackSourceId>(attackSourceId)};

    auto attackSource{hooks::getAttackSourceById(static_cast<game::AttackSourceId>(attackSourceId))};

    if (!attackSource) {
        return;
    }

    const CMidUnit* targetUnit = fn.findUnitById(objectMap, &unitId.id);
    const IUsSoldier* targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);
    const LImmuneCat* immuneCat = targetSoldier->vftable->getImmuneByAttackSource(targetSoldier,
                                                                                  attackSource);

    if (immuneCat->id == ImmuneCategories::get().always->id)
        return;

    BattleMsgDataApi::get().removeUnitAttackSourceWard(battle, &unitId.id, attackSource);
}

void BattleMsgDataView::removeAttackClassWard(const IdView& unitId, int attackClassId)
{
    using namespace game;
    auto& fn = gameFunctions();

    IMidgardObjectMap* objectMap = const_cast<game::IMidgardObjectMap*>(hooks::getObjectMap());
    BattleMsgData* battle = const_cast<game::BattleMsgData*>(battleMsgData);

    const LAttackClass* attackClass = hooks::getAttackClassById(static_cast<AttackClassId>(attackClassId));
    if (!attackClass) {
        return;
    }

    const CMidUnit* targetUnit = fn.findUnitById(objectMap, &unitId.id);
    const IUsSoldier* targetSoldier = fn.castUnitImplToSoldier(targetUnit->unitImpl);
    const LImmuneCat* immuneCat = targetSoldier->vftable->getImmuneByAttackClass(targetSoldier,
                                                                                 attackClass);

    if (immuneCat->id == ImmuneCategories::get().always->id)
        return;


    BattleMsgDataApi::get().removeUnitAttackClassWard(battle, &unitId.id, attackClass);
}

void BattleMsgDataView::setRevivedStatus(const IdView& unitId, bool status)
{
    using namespace game;

    UnitInfo* info = BattleMsgDataApi::get().getUnitInfoById(battleMsgData, &unitId.id);
    if (!info) {
        return;
    }
    
    info->unitFlags.parts.revived = status;

}

} // namespace bindings
