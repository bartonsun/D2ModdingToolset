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

#include "magetowerview.h"
#include "midsitemage.h"
#include "midgardobjectmap.h"
#include <sol/sol.hpp>

namespace bindings {

MageTowerView::MageTowerView(const game::CMidSiteMage* mage,
                             const game::IMidgardObjectMap* objectMap)
    : SiteView(mage, objectMap)
{ }

void MageTowerView::bind(sol::state& lua)
{
    auto view = lua.new_usertype<MageTowerView>("MageTowerView", sol::base_classes,
                                                sol::bases<SiteView>());
    bindAccessMethods(view);
    view["spells"] = sol::property(&MageTowerView::getSpells);
    view["AddSpell"] = sol::overload<>(&MageTowerView::addSpell, &MageTowerView::addSpellByString);
    view["RemoveSpell"] = sol::overload<>(&MageTowerView::removeSpell, &MageTowerView::removeSpellByString);
}

std::vector<IdView> MageTowerView::getSpells() const
{
    using namespace game;

    const CMidSiteMage* mage = static_cast<const game::CMidSiteMage*>(site);

    std::vector<IdView> spells;
    spells.reserve(mage->spells.length);

    for (const auto& spellId : mage->spells) {
        spells.emplace_back(spellId);
    }

    return spells;
}

bool MageTowerView::addSpell(const IdView& spellId)
{
    using namespace game;

    CMidSiteMage* mage = const_cast<CMidSiteMage*>(static_cast<const CMidSiteMage*>(site));
    const auto& idListApi = IdListApi::get();

    // Prevent duplicates: check if spell is already in the list
    IdListIterator it{};
    idListApi.find(&it, mage->spells.begin(), mage->spells.end(), &spellId.id);
    if (it != mage->spells.end()) {
        return false;
    }

    idListApi.pushBack(&mage->spells, &spellId.id);

    objectMap->vftable->findScenarioObjectByIdForChange(const_cast<IMidgardObjectMap*>(objectMap),
                                                        &site->id);

    return true;
}

bool MageTowerView::addSpellByString(const std::string& spellId)
{
    return addSpell(IdView{spellId});
}

bool MageTowerView::removeSpell(const IdView& spellId)
{
    using namespace game;

    CMidSiteMage* mage = const_cast<CMidSiteMage*>(static_cast<const CMidSiteMage*>(site));
    const auto& idListApi = IdListApi::get();

    IdListIterator it{};
    idListApi.find(&it, mage->spells.begin(), mage->spells.end(), &spellId.id);
    if (it == mage->spells.end()) {
        return false;
    }
    idListApi.erase(&mage->spells, it);

    objectMap->vftable->findScenarioObjectByIdForChange(const_cast<IMidgardObjectMap*>(objectMap),
                                                        &site->id);

    return true;
}

bool MageTowerView::removeSpellByString(const std::string& spellId)
{
    return removeSpell(IdView{spellId});
}

} // namespace bindings
