/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Vladimir Makeev.
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

#include "midgardplan.h"
#include "version.h"
#include <array>

namespace game::CMidgardPlanApi {

// clang-format off
static std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::GetObjectId)0x5f685f,
        (Api::IsPositionContainsObjects)0x5f69ae,
        (Api::CanPlaceSite)nullptr,
        (Api::AddMapElement)nullptr,
        (Api::GetObjectsAtPoint)nullptr,
    },
    // Russobit
    Api{
        (Api::GetObjectId)0x5f685f,
        (Api::IsPositionContainsObjects)0x5f69ae,
        (Api::CanPlaceSite)nullptr,
        (Api::AddMapElement)nullptr,
        (Api::GetObjectsAtPoint)nullptr,
    },
    // Gog
    Api{
        (Api::GetObjectId)0x5f54e2,
        (Api::IsPositionContainsObjects)0x5f5631,
        (Api::CanPlaceSite)nullptr,
        (Api::AddMapElement)nullptr,
        (Api::GetObjectsAtPoint)nullptr,
    },
    // Scenario Editor
    Api{
        (Api::GetObjectId)0x4e4a42,
        (Api::IsPositionContainsObjects)0x4e4b91,
        (Api::CanPlaceSite)0x511248,
        (Api::AddMapElement)0x4e4e15,
        (Api::GetObjectsAtPoint)0x4e4cf7,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CMidgardPlanApi
