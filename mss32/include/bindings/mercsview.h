/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Vladimir Makeev.
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

#ifndef MERCSVIEW_H
#define MERCSVIEW_H

#include "siteview.h"
#include "unitimplview.h"

namespace game {
struct CMidSiteMercs;
struct MercenaryUnit;
}

namespace bindings {

class MercenaryUnitView
{
public:
    MercenaryUnitView(game::MercenaryUnit* mercUnit);
    
    static void bind(sol::state& lua);

    UnitImplView getImpl() const;
    bool isUnique() const;
    int getLevel() const;
    void setUnique(bool value);

private:
    game::MercenaryUnit* mercUnit;
};

class MercsView : public SiteView
{
public:
    MercsView(const game::CMidSiteMercs* mercs, const game::IMidgardObjectMap* objectMap);

    static void bind(sol::state& lua);

    std::vector<MercenaryUnitView> getUnits() const;

    bool addUnit(const IdView& unitImplId, int level, bool unique);
    bool addUnitByString(const std::string& id, int level, bool unique);

    bool removeUnit(const IdView& unitImplId, int level);
    bool removeUnitById(const std::string& id, int level);
};

} // namespace bindings

#endif // MERCSVIEW_H
