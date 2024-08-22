/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2020 Vladimir Makeev.
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

#include "menubase.h"
#include "version.h"
#include <array>

namespace game::CMenuBaseApi {

// clang-format off
static std::array<Api, 3> functions = {{
    // Akella
    Api{
        (Api::Constructor)0x4db5ea,
        (Api::Destructor)0x4db6f7,
        (Api::CreateMenu)0x4db76a,
        (Api::GetDialogInterface)0x40306a,
        (Api::ButtonCallback)0x4dcf9e,
        (Api::CreateButtonFunctor)0x4ea842,
        (Api::CreateListBoxDisplayFunctor)0x4e563f,
        (Api::CreateListBoxDisplayTextFunctor)0x4ea88e,
        (Api::ListBoxCallback)0x4e9cb3,
        (Api::CreateListBoxFunctor)0x4ea8da,
        (Api::CreateSpinButtonFunctor)0x4e5867,
        (Api::CreatePictureFunctor)0x492cb0,
        (Api::CreateRadioButtonFunctor)0x4e833a,
    },
    // Russobit
    Api{
        (Api::Constructor)0x4db5ea,
        (Api::Destructor)0x4db6f7,
        (Api::CreateMenu)0x4db76a,
        (Api::GetDialogInterface)0x40306a,
        (Api::ButtonCallback)0x4dcf9e,
        (Api::CreateButtonFunctor)0x4ea842,
        (Api::CreateListBoxDisplayFunctor)0x4e563f,
        (Api::CreateListBoxDisplayTextFunctor)0x4ea88e,
        (Api::ListBoxCallback)0x4e9cb3,
        (Api::CreateListBoxFunctor)0x4ea8da,
        (Api::CreateSpinButtonFunctor)0x4e5867,
        (Api::CreatePictureFunctor)0x492cb0,
        (Api::CreateRadioButtonFunctor)0x4e833a,
    },
    // Gog
    Api{
        (Api::Constructor)0x4dac4f,
        (Api::Destructor)0x4dad5c,
        (Api::CreateMenu)0x4dadcf,
        (Api::GetDialogInterface)0x402db0,
        (Api::ButtonCallback)0x4dbeae,
        (Api::CreateButtonFunctor)0x4e9cdd,
        (Api::CreateListBoxDisplayFunctor)0x4e4d4c,
        (Api::CreateListBoxDisplayTextFunctor)0x4e9d29,
        (Api::ListBoxCallback)0x4e914e,
        (Api::CreateListBoxFunctor)0x4e9d75,
        (Api::CreateSpinButtonFunctor)0x4e4f74,
        (Api::CreatePictureFunctor)0x492766,
        (Api::CreateRadioButtonFunctor)0x4e7839,
    },
}};

static std::array<CMenuBaseVftable*, 3> vftables = {{
    // Akella
    (CMenuBaseVftable*)0x6dd294,
    // Russobit
    (CMenuBaseVftable*)0x6dd294,
    // Gog
    (CMenuBaseVftable*)0x6db234,
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

const CMenuBaseVftable* vftable()
{
    return vftables[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CMenuBaseApi
