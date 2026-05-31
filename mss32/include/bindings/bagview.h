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

#ifndef BAGTVIEW_H
#define BAGTVIEW_H

#include "groupview.h"
#include "idview.h"
#include "playerview.h"
#include "point.h"
#include <optional>
#include <vector>

namespace sol {
class state;
}

namespace game {
struct CMidBag;
struct IMidgardObjectMap;
} // namespace game

namespace bindings {

class ItemView;

class BagView
{
public:
    BagView(const game::CMidBag* bag, const game::IMidgardObjectMap* objectMap);

    static void bind(sol::state& lua);

    IdView getId() const;
    Point getPosition() const;
    int BagView::getImage();
    std::vector<ItemView> getInventoryItems() const;

    bool addItem(const IdView& itemId, int amount);
    bool addItemByString(const std::string& itemId, int amount);

private:
    const game::CMidBag* bag;
    const game::IMidgardObjectMap* objectMap;
};

} // namespace bindings

#endif // BAGVIEW_H
