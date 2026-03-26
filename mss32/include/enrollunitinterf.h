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

#ifndef ENROLLUNITINTERF_H
#define ENROLLUNITINTERF_H

#include "idlist.h"
#include "Idvector.h"

namespace game {

struct CEnrollUnitInterf;
struct CDialogInterf;

namespace EnrollUnitInterfApi {

struct Api
{
    using GetDialog = CDialogInterf*(__thiscall*)(CEnrollUnitInterf* thisptr);
    GetDialog getDialog;

    using Constructor = CEnrollUnitInterf*(__thiscall*)(CEnrollUnitInterf* thisptr,
                                                        int a2,
                                                        int functor,
                                                        int a4,
                                                        int vfDelta,
                                                        game::IdVector* unitsVector,
                                                        bool isHireUnit);
    Constructor constructor;
};

Api& get();

} // namespace EnrollUnitInterfApi

} // namespace game

#endif // ENROLLUNITINTERF_H
