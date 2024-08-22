/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Stanislav Egorov.
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

#include "midstack.h"
#include "version.h"
#include <array>

namespace game::CMidStackApi {

// clang-format off
static std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::SetPosition)0x5edc57,
        (Api::SetOwner)0x5ee191,
    },
    // Russobit
    Api{
        (Api::SetPosition)0x5edc57,
        (Api::SetOwner)0x5ee191,
    },
    // Gog
    Api{
        (Api::SetPosition)0x5ec917,
        (Api::SetOwner)0x5ece51,
    },
    // Scenario Editor
    Api{
        (Api::SetPosition)0x4f14d0,
        (Api::SetOwner)0x4f1907,
    },
}};

static std::array<const CMidStackIMapElementVftable*, 4> vftables = {{
    // Akella
    (const CMidStackIMapElementVftable*)0x6f0a94,
    // Russobit
    (const CMidStackIMapElementVftable*)0x6f0a94,
    // Gog
    (const CMidStackIMapElementVftable*)0x6eea34,
    // Scenario Editor
    (const CMidStackIMapElementVftable*)0x5da744,
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

const CMidStackIMapElementVftable* vftable()
{
    return vftables[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CMidStackApi
