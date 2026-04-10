/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2026 Alexey Voskresensky.
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

#include "globalunitsview.h"
#include "idview.h"
#include "unitimplview.h"
#include "unitutils.h"
#include "usunitimpl.h"
#include <sol/sol.hpp>

namespace bindings {

void GlobalUnitsView::bind(sol::state& lua)
{
    auto view = lua.new_usertype<GlobalUnitsView>("GlobalUnitsView");
    view["getBaseImpl"] = &GlobalUnitsView::getBaseImpl;
}

std::optional<UnitImplView> GlobalUnitsView::getBaseImpl(const std::string& idStr) const
{
    IdView idView(idStr);
    game::TUsUnitImpl* rawImpl = hooks::getGlobalUnitImpl(&idView.id);
    if (!rawImpl) {
        return std::nullopt;
    }
    const game::IUsUnit* baseImpl = static_cast<const game::IUsUnit*>(rawImpl);
    return UnitImplView(baseImpl);
}

} // namespace bindings