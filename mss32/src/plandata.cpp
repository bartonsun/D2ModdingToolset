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

#include "plandata.h"
#include "version.h"
#include <array>

namespace game::PlanDataApi {

// clang-format off
std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::Ctor)0x419a8f,
        (Api::Dtor)0x419aee,
    },
    // Russobit
    Api{
        (Api::Ctor)0x419a8f,
        (Api::Dtor)0x419aee,
    },
    // Gog
    Api{
        (Api::Ctor)0x50ba21,
        (Api::Dtor)0x50bD7e,
    },
    // Scenario Editor
    Api{
        (Api::Ctor)0, //TODO
        (Api::Dtor)0, //TODO
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::PlanDataApi
