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

#ifndef MAGETOWERVIEW_H
#define MAGETOWERVIEW_H

#include "idview.h"
#include "siteview.h"

namespace game {
struct CMidSiteMage;
}

namespace bindings {

class MageTowerView : public SiteView
{
public:
    MageTowerView(const game::CMidSiteMage* mage, const game::IMidgardObjectMap* objectMap);

    static void bind(sol::state& lua);

    std::vector<IdView> getSpells() const;

    bool addSpell(const IdView& spellId);
    bool addSpellByString(const std::string& spellId);

    bool removeSpell(const IdView& spellId);
    bool removeSpellByString(const std::string& spellId);
};

} // namespace bindings

#endif // MAGETOWERVIEW_H
