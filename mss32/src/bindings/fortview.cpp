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

#include "fortview.h"
#include "fortification.h"
#include "gameutils.h"
#include "itemview.h"
#include "hooks.h"
#include "midgardobjectmap.h"
#include "midsubrace.h"
#include "stackview.h"
#include "visitors.h"
#include "miditem.h"
#include "midinventory.h"
#include "idvector.h"
#include <sol/sol.hpp>

namespace bindings {

FortView::FortView(const game::CFortification* fort, const game::IMidgardObjectMap* objectMap)
    : fort{fort}
    , objectMap{objectMap}
{ }

void FortView::bind(sol::state& lua)
{
    auto fortView = lua.new_usertype<FortView>("FortView");
    fortView["id"] = sol::property(&FortView::getId);
    fortView["position"] = sol::property(&FortView::getPosition);
    fortView["entrance"] = sol::property(&FortView::getEntrance);
    fortView["owner"] = sol::property(&FortView::getOwner);
    fortView["group"] = sol::property(&FortView::getGroup);
    fortView["visitor"] = sol::property(&FortView::getVisitor);
    fortView["subrace"] = sol::property(&FortView::getSubrace);
    fortView["inventory"] = sol::property(&FortView::getInventoryItems);
    fortView["capital"] = sol::property(&FortView::isCapital);
    fortView["tier"] = sol::property(&FortView::getTier);
    fortView["AddItem"] = sol::overload<>(&FortView::addItem, &FortView::addItemByString);
}

IdView FortView::getId() const
{
    return IdView{fort->id};
}

Point FortView::getPosition() const
{
    return Point{fort->mapElement.position};
}

Point FortView::getEntrance() const
{
    const auto& mapElement{fort->mapElement};
    const auto& position{mapElement.position};

    return Point{hooks::getObjectEntrance(position, mapElement.sizeX, mapElement.sizeY)};
}

std::optional<PlayerView> FortView::getOwner() const
{
    auto player = hooks::getPlayer(objectMap, &fort->ownerId);
    if (!player) {
        return std::nullopt;
    }

    return PlayerView{player, objectMap};
}

GroupView FortView::getGroup() const
{
    return GroupView{&fort->group, objectMap, &fort->id};
}

std::optional<StackView> FortView::getVisitor() const
{
    auto stack = hooks::getStack(objectMap, &fort->stackId);
    if (!stack)
        return std::nullopt;

    return {StackView{stack, objectMap}};
}

int FortView::getSubrace() const
{
    auto obj{objectMap->vftable->findScenarioObjectById(objectMap, &fort->subraceId)};
    auto subrace{static_cast<const game::CMidSubRace*>(obj)};

    return subrace ? static_cast<int>(subrace->subraceCategory.id) : game::emptyCategoryId;
}

std::vector<ItemView> FortView::getInventoryItems() const
{
    const auto& items = fort->inventory.items;
    const auto count = items.end - items.bgn;

    std::vector<ItemView> result;
    result.reserve(count);

    for (const game::CMidgardID* it = items.bgn; it != items.end; it++) {
        result.push_back(ItemView{it, objectMap});
    }

    return result;
}

bool FortView::isCapital() const
{
    return getTier() == 6;
}

int FortView::getTier() const
{
    const auto* vftable{static_cast<const game::CFortificationVftable*>(fort->vftable)};

    return vftable->getTier(fort, objectMap);
}

bool FortView::addItem(const IdView& itemId, int amount)
{
    using namespace game;

    if (amount == 0)
        return false;

    static const auto& visitor = VisitorApi::get();

    const auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    if (amount > 0) {
        for (int i = 0; i < amount; i++) {
            if (!visitor.createItem(&fort->id, &itemId.id, obj, 1))
                return false;
        }
        return true;
    }

    const int toRemove = -amount;
    int removed = 0;

    const CMidItem* item = static_cast<const CMidItem*>(objectMap->vftable->findScenarioObjectById(objectMap, &itemId.id));

    if (item == nullptr) {
        auto inv = fort->inventory;
        const int count = static_cast<int>(inv.items.end - inv.items.bgn);
        for (int i = count - 1; i >= 0 && removed < toRemove; i--) {
            const CMidgardID& instanceId = inv.items.bgn[i];

            const IMidScenarioObject* obj2 = objectMap->vftable->findScenarioObjectById(objectMap, &instanceId);
            if (!obj2)
                continue;

            const CMidItem* invItem = static_cast<const CMidItem*>(obj2);
            if (invItem->globalItemId != itemId.id)
                continue;

            if (!visitor.destroyItem(&fort->id, &instanceId, obj, 1))
                return false;

            removed++;
        }
    } else {
        return visitor.destroyItem(&fort->id, &itemId.id, obj, 1);
    }

    return removed == toRemove;
}

bool FortView::addItemByString(const std::string& itemId, int amount)
{
    return addItem(IdView{itemId}, amount);
}

} // namespace bindings
