/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Vladimir Makeev.
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

#include "editboxinterf.h"
#include "version.h"
#include <array>

namespace game::CEditBoxInterfApi {

// clang-format off
static std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::SetFilterAndLength)0x5c9c86,
        (Api::SetInputLength)0x5390dc,
        (Api::SetString)0x5391f9,
        (Api::Update)0x539486,
        (Api::UpdateFocus)0x5389e6,
        (Api::IsCharValid)0x538471,
        (Api::EditBoxDataCtor)0x5377aa,
        (Api::GetTextCursorPosIdx)0x5378a0,
        (Api::SetEditable)0x539104,
    },
    // Russobit
    Api{
        (Api::SetFilterAndLength)0x5c9c86,
        (Api::SetInputLength)0x5390dc,
        (Api::SetString)0x5391f9,
        (Api::Update)0x539486,
        (Api::UpdateFocus)0x5389e6,
        (Api::IsCharValid)0x538471,
        (Api::EditBoxDataCtor)0x5377aa,
        (Api::GetTextCursorPosIdx)0x5378a0,
        (Api::SetEditable)0x539104,
    },
    // Gog
    Api{
        (Api::SetFilterAndLength)0x5c8c54,
        (Api::SetInputLength)0x5386e4,
        (Api::SetString)0x538801,
        (Api::Update)0x538a8e,
        (Api::UpdateFocus)0x537fee,
        (Api::IsCharValid)0x537a79,
        (Api::EditBoxDataCtor)0x536da5,
        (Api::GetTextCursorPosIdx)0x536e9b,
        (Api::SetEditable)0x53870c,
    },
    // Scenario Editor
    Api{
        (Api::SetFilterAndLength)0x4d18b8,
        (Api::SetInputLength)0x492cb3,
        (Api::SetString)0x492ddb,
        (Api::Update)0x493068,
        (Api::UpdateFocus)0x4925bd,
        (Api::IsCharValid)0x492048,
        (Api::EditBoxDataCtor)0x491340,
        (Api::GetTextCursorPosIdx)0x491436,
        (Api::SetEditable)0x492cdb,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CEditBoxInterfApi
