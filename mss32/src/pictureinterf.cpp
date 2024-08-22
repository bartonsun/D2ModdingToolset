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

#include "pictureinterf.h"
#include "version.h"
#include <array>

namespace game::CPictureInterfApi {

// clang-format off
static std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::SetImage)0x5318a0,
        (Api::SetImageWithAnchor)0x5318ea,
        (Api::AssignFunctor)0x531843,
    },
    // Russobit
    Api{
        (Api::SetImage)0x5318a0,
        (Api::SetImageWithAnchor)0x5318ea,
        (Api::AssignFunctor)0x531843,
    },
    // Gog
    Api{
        (Api::SetImage)0x530db8,
        (Api::SetImageWithAnchor)0x530e29,
        (Api::AssignFunctor)0x530d5b,
    },
    // Scenario Editor
    Api{
        (Api::SetImage)0x494988,
        (Api::SetImageWithAnchor)0x4949d2,
        (Api::AssignFunctor)0,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CPictureInterfApi
