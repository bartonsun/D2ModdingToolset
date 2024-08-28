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

#include "midgard.h"
#include "version.h"
#include <array>

namespace game::CMidgardApi {

// clang-format off
static std::array<Api, 3> functions = {{
    // Akella
    Api{
        (Api::Instance)0x401d35,
        (Api::SetClientsNetProxy)0x4030c5,
        (Api::SetNetService)0x403089,
        (Api::CreateNetClient)0x403200,
        (Api::SendNetMsgToServer)0x403b71,
        (Api::ClearNetworkState)0x402fc2,
        (Api::ClearNetworkStateAndService)0x402f24,
        (Api::StartMenuMessageCallback)0x402abf,
    },
    // Russobit
    Api{
        (Api::Instance)0x401d35,
        (Api::SetClientsNetProxy)0x4030c5,
        (Api::SetNetService)0x403089,
        (Api::CreateNetClient)0x403200,
        (Api::SendNetMsgToServer)0x403b71,
        (Api::ClearNetworkState)0x402fc2,
        (Api::ClearNetworkStateAndService)0x402f24,
        (Api::StartMenuMessageCallback)0x402abf,
    },
    // Gog
    Api{
        (Api::Instance)0x401a7b,
        (Api::SetClientsNetProxy)0x402e0b,
        (Api::SetNetService)0x402dcf,
        (Api::CreateNetClient)0x402f46,
        (Api::SendNetMsgToServer)0x4038b7,
        (Api::ClearNetworkState)0x402d08,
        (Api::ClearNetworkStateAndService)0x402c6a,
        (Api::StartMenuMessageCallback)0x402805,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CMidgardApi
