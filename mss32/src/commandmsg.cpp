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

#include "commandmsg.h"
#include "version.h"
#include <array>

namespace game::CCommandMsgApi {

// clang-format off
static std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::Constructor)0x47b60f,
        (Api::Destructor)0x47b64d,
        (Api::Serialize)0x47f663,
        (Api::Create)0x47b9ba,
    },
    // Russobit
    Api{
        (Api::Constructor)0x47b60f,
        (Api::Destructor)0x47b64d,
        (Api::Serialize)0x47f663,
        (Api::Create)0x47b9ba,
    },
    // Gog
    Api{
        (Api::Constructor)0x47b146,
        (Api::Destructor)0x47b184,
        (Api::Serialize)0x47b1df,
        (Api::Create)0x47b538,
    },
    // Scenario Editor
    Api{
        (Api::Constructor)nullptr,
        (Api::Destructor)nullptr,
        (Api::Serialize)nullptr,
        (Api::Create)nullptr,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CCommandMsgApi
