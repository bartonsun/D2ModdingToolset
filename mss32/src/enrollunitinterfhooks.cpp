/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2026 Alexey Voskresensky.
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

#include "button.h"
#include "dialoginterf.h"
#include "enrollunitinterf.h"
#include "enrollunitinterfhooks.h"
#include "midgardid.h"
#include "originalfunctions.h"
#include "idvector.h"
#include <spdlog/spdlog.h>

namespace hooks {

game::CEnrollUnitInterf* __fastcall enrollUnitInterfCtorHooked(game::CEnrollUnitInterf* thisptr,
                                                               int /*edx*/,
                                                               int a2,
                                                               int functor,
                                                               int a4,
                                                               int vfDelta,
                                                               game::IdVector* unitsVector,
                                                               bool isHireUnit)
{
    using namespace game;

    auto result = getOriginalFunctions().enrollUnitInterfCtor(thisptr, a2, functor, a4, vfDelta,
                                                              unitsVector, isHireUnit);

    if (unitsVector->size() == 0) {
        auto dialog = EnrollUnitInterfApi::get().getDialog(thisptr);
        auto button = game::CDialogInterfApi::get().findButton(dialog, "BTN_HIRE_LEADER");
        if (button) {
            button->vftable->setEnabled(button, false);
        }
    }

    return result;
}

} // namespace hooks