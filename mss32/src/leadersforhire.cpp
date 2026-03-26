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

#include "leadersforhire.h"
#include "version.h"
#include <array>

namespace game::LeadersForHireApi {

// clang-format off
static std::array<Api, 3> functions = {{
    // Akella
    Api{
        (Api::GetLeadersHireList)0x5d5835,
        (Api::AddNobleLeaderToUI)0x4b5131,
        (Api::ChangeStackLeaderInCapital)0x4225cd,
    },
    // Russobit
    Api{
        (Api::GetLeadersHireList)0x5d5835,
        (Api::AddNobleLeaderToUI)0x4b5131,
        (Api::ChangeStackLeaderInCapital)0x4225cd,
    },
    // Gog
    Api{
        (Api::GetLeadersHireList)0x5d475e,
        (Api::AddNobleLeaderToUI)0x4b47ca,
        (Api::ChangeStackLeaderInCapital)0x4220eb,
    }
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::LeadersForHireApi
