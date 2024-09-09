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

#include "midcommandqueue2.h"
#include "version.h"
#include <array>

namespace game::CMidCommandQueue2Api {

// clang-format off
static std::array<Api, 3> functions = {{
    // Akella
    Api{
        (Api::ProcessCommands)0x410678,
        (Api::NMMapConstructor)0x40fd60,
        (CNetMsgMapEntry_member::Callback)0x4102b0,
        (Api::Front)0x4105a4,
    },
    // Russobit
    Api{
        (Api::ProcessCommands)0x410678,
        (Api::NMMapConstructor)0x40fd60,
        (CNetMsgMapEntry_member::Callback)0x4102b0,
        (Api::Front)0x4105a4,
    },
    // Gog
    Api{
        (Api::ProcessCommands)0x410236,
        (Api::NMMapConstructor)0x40f91e,
        (CNetMsgMapEntry_member::Callback)0x40fe6e,
        (Api::Front)0x410162,
    }
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CMidCommandQueue2Api
