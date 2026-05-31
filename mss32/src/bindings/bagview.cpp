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

#include "midbag.h"
#include "bagview.h"
#include "gameutils.h"
#include "hooks.h"
#include "itemview.h"
#include "midgardobjectmap.h"
#include "midsubrace.h"
#include "stackview.h"
#include "visitors.h"
#include "miditem.h"
#include <sol/sol.hpp>

namespace bindings {

BagView::BagView(const game::CMidBag* bag, const game::IMidgardObjectMap* objectMap)
    : bag{bag}
    , objectMap{objectMap}
{ }

void BagView::bind(sol::state& lua)
{
    auto bagView = lua.new_usertype<BagView>("BagView");
    bagView["id"] = sol::property(&BagView::getId);
    bagView["position"] = sol::property(&BagView::getPosition);
    bagView["image"] = sol::property(&BagView::getImage);
    bagView["inventory"] = sol::property(&BagView::getInventoryItems);
    bagView["AddItem"] = sol::overload<>(&BagView::addItem, &BagView::addItemByString);
}

IdView BagView::getId() const
{
    return IdView{bag->id};
}

Point BagView::getPosition() const
{
    return Point{bag->mapElement.position};
}

int BagView::getImage()
{
    return bag->image;
}

std::vector<ItemView> BagView::getInventoryItems() const
{
    const auto& items = bag->inventory.items;
    const auto count = items.end - items.bgn;

    std::vector<ItemView> result;
    result.reserve(count);

    for (const game::CMidgardID* it = items.bgn; it != items.end; it++) {
        result.push_back(ItemView{it, objectMap});
    }

    return result;
}

bool BagView::addItem(const IdView& itemId, int amount)
{
    using namespace game;

    if (amount == 0)
        return false;

    static const auto& visitor = VisitorApi::get();

    const auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    if (amount > 0) {
        for (int i = 0; i < amount; i++) {
            if (!visitor.createItem(&bag->id, &itemId.id, obj, 1))
                return false;
        }
        return true;
    }

    const int toRemove = -amount;
    int removed = 0;

    const CMidItem* item = static_cast<const CMidItem*>(
        objectMap->vftable->findScenarioObjectById(objectMap, &itemId.id));

    if (item == nullptr) {
        auto inv = bag->inventory;
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

            if (!visitor.destroyItem(&bag->id, &instanceId, obj, 1))
                return false;

            removed++;
        }
    } else {
        return visitor.destroyItem(&bag->id, &itemId.id, obj, 1);
    }

    return removed == toRemove;
}

bool BagView::addItemByString(const std::string& itemId, int amount)
{
    return addItem(IdView{itemId}, amount);
}


} // namespace bindings
