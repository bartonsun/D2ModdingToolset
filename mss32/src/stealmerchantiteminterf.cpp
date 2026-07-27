/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/bartonsun/D2ModdingToolset)
 * Copyright (C) 2026 Max Vynogradov.
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
// ============================================================
// File: stealmerchantiteminterf.cpp
// ============================================================

#include "stealmerchantiteminterf.h"
#include "version.h"

#include <array>

namespace game::StealMerchantItemInterfApi {

// clang-format off

static std::array<Api, 4> functions = {{

    // Akella
    Api{
        (Api::Constructor)0x4A519E,
        (Api::GetListBox)0x4A56CE,
        (Api::AddStealItem)0x4A5BCA
    },

    // Russobit
    Api{
        (Api::Constructor)0x4A519E,
        (Api::GetListBox)0x4A56CE,
        (Api::AddStealItem)0x4A5BCA
    },

    // Gog
    Api{
        (Api::Constructor)0x4A4A1F,
        (Api::GetListBox)0x4A4F2F,
        (Api::AddStealItem)0x4A542B
    },

    // Scenario Editor
    Api{
        (Api::Constructor)nullptr,
        (Api::GetListBox)nullptr,
        (Api::AddStealItem)nullptr
    }

}};

// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::StealMerchantItemInterfApi