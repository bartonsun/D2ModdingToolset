/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Stanislav Egorov.
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

#include "phasegame.h"
#include "version.h"
#include <array>

namespace game::CPhaseGameApi {

// clang-format off
static std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::CheckObjectLock)0x4078b7,
        (Api::SendStackMoveMsg)0x40650f,
    },
    // Russobit
    Api{
        (Api::CheckObjectLock)0x4078b7,
        (Api::SendStackMoveMsg)0x40650f,
    },
    // Gog
    Api{
        (Api::CheckObjectLock)0x40753e,
        (Api::SendStackMoveMsg)0x40619b,
    },
    // Scenario Editor
    Api{
        (Api::CheckObjectLock)nullptr,
        (Api::SendStackMoveMsg)nullptr,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CPhaseGameApi