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

#include "displayhandlers.h"
#include "version.h"
#include <array>

namespace game {
namespace DisplayHandlersApi {

// clang-format off
static std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::DisplayHandler<CMidVillage>)0x5bcb73,
        (Api::DisplayHandler<CMidSite>)0x5bd31a,
    },
    // Russobit
    Api{
        (Api::DisplayHandler<CMidVillage>)0x5bcb73,
        (Api::DisplayHandler<CMidSite>)0x5bd31a,
    },
    // Gog
    Api{
        (Api::DisplayHandler<CMidVillage>)0x5bbc37,
        (Api::DisplayHandler<CMidSite>)0x5bc3de,
    },
    // Scenario Editor
    Api{
        (Api::DisplayHandler<CMidVillage>)0x55d9eb,
        (Api::DisplayHandler<CMidSite>)0x55e192,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace DisplayHandlersApi

namespace ImageDisplayHandlerApi {

// clang-format off
static std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::AddHandler)0x5be078,
    },
    // Russobit
    Api{
        (Api::AddHandler)0x5be078,
    },
    // Gog
    Api{
        (Api::AddHandler)0x5bd13c,
    },
    // Scenario Editor
    Api{
        (Api::AddHandler)0x55ef15,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

// clang-format off
static std::array<ImageDisplayHandlerMap*, 4> instances = {{
    // Akella
    (ImageDisplayHandlerMap*)0x83a8d0,
    // Russobit
    (ImageDisplayHandlerMap*)0x83a8d0,
    // Gog
    (ImageDisplayHandlerMap*)0x838878,
    // Scenario Editor
    (ImageDisplayHandlerMap*)0x666440,
}};
// clang-format on

ImageDisplayHandlerMap* instance()
{
    return instances[static_cast<int>(hooks::gameVersion())];
}

} // namespace ImageDisplayHandlerApi

} // namespace game
