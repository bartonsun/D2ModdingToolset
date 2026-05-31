/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Vladimir Makeev.
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

#include "stackview.h"
#include "fortview.h"
#include "game.h"
#include "gameutils.h"
#include "itemview.h"
#include "midgardobjectmap.h"
#include "midstack.h"
#include "midsubrace.h"
#include "unitview.h"
#include "visitors.h"
#include "unitgenerator.h"
#include "usunitimpl.h"
#include "ussoldier.h"
#include "midunit.h"
#include <sol/sol.hpp>
#include <hooks.h>
#include <usstackleader.h>
#include <scenarioinfo.h>
#include <midgard.h>
#include <midserver.h>
#include "midserverlogic.h"
#include "miditem.h"

namespace bindings {

StackView::StackView(const game::CMidStack* stack, const game::IMidgardObjectMap* objectMap)
    : stack{stack}
    , objectMap{objectMap}
{ }

void StackView::bind(sol::state& lua)
{
    auto stackView = lua.new_usertype<StackView>("StackView");
    stackView["id"] = sol::property(&StackView::getId);
    stackView["position"] = sol::property(&StackView::getPosition);
    stackView["owner"] = sol::property(&StackView::getOwner);
    stackView["inside"] = sol::property(&StackView::getInside);
    stackView["group"] = sol::property(&StackView::getGroup);
    stackView["leader"] = sol::property(&StackView::getLeader);
    stackView["movement"] = sol::property(&StackView::getMovement);
    stackView["subrace"] = sol::property(&StackView::getSubrace);
    stackView["invisible"] = sol::property(&StackView::isInvisible);
    stackView["battlesWon"] = sol::property(&StackView::getBattlesWon);
    stackView["skillpoint"] = sol::property(&StackView::getSkillPoint);
    stackView["name"] = sol::property(&StackView::gåtName);

    stackView["inventory"] = sol::property(&StackView::getInventoryItems);
    stackView["getEquippedItem"] = &StackView::getLeaderEquippedItem;
    stackView["order"] = sol::property(&StackView::getOrder);
    stackView["orderTargetId"] = sol::property(&StackView::getOrderTargetId);
    stackView["aiOrder"] = sol::property(&StackView::getAiOrder);
    stackView["GiveSkillPoint"] = &StackView::giveSkillPoint;
    stackView["AddItem"] = &StackView::addItem;
    stackView["SetMovement"] = &StackView::setMovement;
    stackView["AddMovement"] = &StackView::addMovement;
    stackView["SetName"] = &StackView::setName;
    stackView["ChangeLeader"] = &StackView::changeStackLeaeder;
    stackView["SetInvisible"] = &StackView::setInvisible;
}

IdView StackView::getId() const
{
    return IdView{stack->id};
}

Point StackView::getPosition() const
{
    return Point{stack->position};
}

std::optional<PlayerView> StackView::getOwner() const
{
    auto player = hooks::getPlayer(objectMap, &stack->ownerId);
    if (!player) {
        return std::nullopt;
    }

    return PlayerView{player, objectMap};
}

std::optional<FortView> StackView::getInside() const
{
    auto fort = hooks::getFort(objectMap, &stack->insideId);
    if (!fort) {
        return std::nullopt;
    }

    return {FortView{fort, objectMap}};
}

GroupView StackView::getGroup() const
{
    return GroupView{&stack->group, objectMap, &stack->id};
}

std::optional<UnitView> StackView::getLeader() const
{
    auto leaderUnit{game::gameFunctions().findUnitById(objectMap, &stack->leaderId)};
    if (!leaderUnit) {
        return std::nullopt;
    }

    return UnitView{leaderUnit};
}

int StackView::getMovement() const
{
    return stack->movement;
}

int StackView::getSubrace() const
{
    auto obj{objectMap->vftable->findScenarioObjectById(objectMap, &stack->subraceId)};
    auto subrace{static_cast<const game::CMidSubRace*>(obj)};

    return subrace ? static_cast<int>(subrace->subraceCategory.id) : game::emptyCategoryId;
}

bool StackView::isInvisible() const
{
    return stack->invisible;
}

int StackView::getBattlesWon() const
{
    return stack->nbBattle;
}

std::vector<ItemView> StackView::getInventoryItems() const
{
    const auto& items = stack->inventory.items;
    const auto count = items.end - items.bgn;

    std::vector<ItemView> result;
    result.reserve(count);

    for (const game::CMidgardID* it = items.bgn; it != items.end; it++) {
        result.push_back(ItemView{it, objectMap});
    }

    return result;
}

std::optional<ItemView> StackView::getLeaderEquippedItem(const game::EquippedItemIdx& idx) const
{
    const auto& items = stack->leaderEquippedItems;
    const auto count = items.end - items.bgn;
    if (idx < 0 || idx >= count)
        return std::nullopt;

    auto itemId = stack->leaderEquippedItems.bgn[idx];
    if (itemId == game::emptyId)
        return std::nullopt;

    return {ItemView{&itemId, objectMap}};
}

int StackView::getOrder() const
{
    return static_cast<int>(stack->order.id);
}

IdView StackView::getOrderTargetId() const
{
    return IdView{stack->orderTargetId};
}

int StackView::getAiOrder() const
{
    return static_cast<int>(stack->aiOrder.id);
}

void StackView::giveSkillPoint(int amout)
{
    using namespace game;

    CMidStack* cStack = const_cast<CMidStack*>(stack);
    cStack->upgCount += amout;
}

int StackView::getSkillPoint() const
{
    return stack->upgCount;
}

std::string StackView::gåtName() const
{
    using namespace game;

    auto& stringApi = StringAndIdApi::get();

    auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    CMidUnit* stackLeader = static_cast<CMidUnit*>(
        objectMap->vftable->findScenarioObjectById(obj, &stack->leaderId));

    return stackLeader->name.string;
}

bool StackView::addItem(const IdView& itemId, int amount)
{
    using namespace game;

    if (amount == 0)
        return false;

    static const auto& visitor = VisitorApi::get();

    const auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    if (amount > 0) {
        for (int i = 0; i < amount; i++) {
            if (!visitor.createItem(&stack->id, &itemId.id, obj, 1))
                return false;
        }
        return true;
    }

    const int toRemove = -amount;
    int removed = 0;

    const CMidItem* item = static_cast<const CMidItem*>(
        objectMap->vftable->findScenarioObjectById(objectMap, &itemId.id));

    if (item == nullptr) {
        auto inv = stack->inventory;
        const int count = static_cast<int>(inv.items.end - inv.items.bgn);
        for (int i = count - 1; i >= 0 && removed < toRemove; i--) {
            const CMidgardID& instanceId = inv.items.bgn[i];

            const IMidScenarioObject* obj2 = objectMap->vftable
                                                 ->findScenarioObjectById(objectMap, &instanceId);
            if (!obj2)
                continue;

            const CMidItem* invItem = static_cast<const CMidItem*>(obj2);
            if (invItem->globalItemId != itemId.id)
                continue;

            if (!visitor.destroyItem(&stack->id, &instanceId, obj, 1))
                return false;

            removed++;
        }
    } else {
        return visitor.destroyItem(&stack->id, &itemId.id, obj, 1);
    }

    return removed == toRemove;
}

bool StackView::setMovement(int value)
{
    using namespace game;
    
    auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    CMidStack* cStack = static_cast<CMidStack*>(
        objectMap->vftable->findScenarioObjectByIdForChange(obj, &stack->id));

    auto leaderUnit{game::gameFunctions().findUnitById(objectMap, &stack->leaderId)};
    auto leader{game::gameFunctions().castUnitImplToStackLeader(leaderUnit->unitImpl)};

    if (!leader)
        return false;

    cStack->movement = std::clamp(value, 0, leader->vftable->getMovement(leader));
    return true;
}

bool StackView::addMovement(int value)
{
    if (value == 0)
        return false;

    using namespace game;

    auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    CMidStack* cStack = static_cast<CMidStack*>(
        objectMap->vftable->findScenarioObjectByIdForChange(obj, &stack->id));

    auto leaderUnit{game::gameFunctions().findUnitById(objectMap, &stack->leaderId)};
    auto leader{game::gameFunctions().castUnitImplToStackLeader(leaderUnit->unitImpl)};

    if (!leader)
        return false;

    cStack->movement = std::clamp(stack->movement + value, 0, leader->vftable->getMovement(leader));
    return true;
}

bool StackView::setName(std::string& name)
{
    using namespace game;

    if (name.empty())
        return false;
    
    auto& stringApi = StringAndIdApi::get();

    auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    CMidUnit* stackLeader = static_cast<CMidUnit*>(
        objectMap->vftable->findScenarioObjectByIdForChange(obj, &stack->leaderId));

    stringApi.setString(&stackLeader->name, name.c_str());

    return true;
}

std::optional<UnitView> StackView::changeStackLeaeder(const IdView& unitImplId, int level)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& globalApi = GlobalDataApi::get();
    const auto globalData = *globalApi.getGlobalData();
    const auto& visitor = VisitorApi::get();

    auto unit = (IUsUnit*)globalApi.findById(globalData->units, &unitImplId.id);
    if (!unit)
        return std::nullopt;

    bool isLeader = fn.castUnitImplToLeader(unit) != nullptr;
    bool isNoble = fn.castUnitImplToNoble(unit) != nullptr;

    if (!isLeader && !isNoble)
        return std::nullopt;

    CMidUnit* leaderUnit{fn.findUnitById(objectMap, &stack->leaderId)};
    IUsSoldier* leader = fn.castUnitImplToSoldier(leaderUnit->unitImpl);

    IUsSoldier* genSoldier = fn.castUnitImplToSoldier(unit);

    auto group = stack->group;
    int position = 0;
    for (size_t i = 0; i < std::size(group.positions); ++i) {
        const auto& unitId{group.positions[i]};
        if (group.positions[i] == stack->leaderId) {
            position = i;
            break;
        }
    }
    if (leader->vftable->getSizeSmall(leader) && !genSoldier->vftable->getSizeSmall(genSoldier))
    {
        if (position % 2 == 0 && group.positions[position + 1] != emptyId)
            return std::nullopt;

        if (position % 2 == 1) {
            if (group.positions[position & ~1] != emptyId)
                return std::nullopt;
            position = position & ~1;
        }
    }

    int lvl = level <= 0 ? leader->vftable->getLevel(leader) : level;

    CUnitGenerator* unitGenerator = globalData->unitGenerator;

    CMidgardID implId = unitImplId.id;

    CMidgardID genId{};
    unitGenerator->vftable->generateUnitImplId(unitGenerator, &genId, &implId, lvl);
    if (genId != implId && unitGenerator->vftable->isUnitGenerated(unitGenerator, &genId)) {
        if (unitGenerator->vftable->generateUnitImpl(unitGenerator, &genId)) {
            implId = genId;
        }
    }

    IMidgardObjectMap* obj = const_cast<IMidgardObjectMap*>(objectMap);

    CMidStack* cStack = static_cast<CMidStack*>(
        objectMap->vftable->findScenarioObjectByIdForChange(obj, &stack->id));

    visitor.extractUnitFromGroup(&cStack->leaderId, &cStack->id, obj, 1);

    const CScenarioInfo* scenarioInfo = hooks::getScenarioInfo(objectMap);
    int creationTurn = scenarioInfo->currentTurn;
    bool result = visitor.addUnitToGroup(&implId, &cStack->id, position, &creationTurn, 1,
                                         obj, 1);

    auto midgard = CMidgardApi::get().instance();
    auto server = midgard->data->server;
    auto serverLogic = server->data->serverLogic;

    CMidStack* updatedStack = static_cast<CMidStack*>(obj->vftable->findScenarioObjectById(obj, &stack->id));

    game::IMidMsgSender* msgSender = static_cast<IMidMsgSender*>(serverLogic);

    {
        cStack->facing = (stack->facing + 1) % 8;
        msgSender->vftable->sendObjectsChanges(msgSender);
    }
    {
        cStack->facing = (stack->facing + 7) % 8;
        msgSender->vftable->sendObjectsChanges(msgSender);
    }
    
    return UnitView(fn.findUnitById(objectMap, &updatedStack->leaderId));
}

bool StackView::setInvisible(const bool value)
{
    using namespace game;

    IMidgardObjectMap* obj = const_cast<IMidgardObjectMap*>(objectMap);

    CMidStack* cStack = static_cast<CMidStack*>(
        objectMap->vftable->findScenarioObjectByIdForChange(obj, &stack->id));

    cStack->invisible = value;

    return true;
}

} // namespace bindings
