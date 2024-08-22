/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2023 Vladimir Makeev.
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

#ifndef NOTIFYPLAYERLIST_H
#define NOTIFYPLAYERLIST_H

#include "d2assert.h"

namespace game {

struct INotifyPlayerListVftable;

struct INotifyPlayerList
{
    INotifyPlayerListVftable* vftable;
};

struct INotifyPlayerListVftable
{
    using Destructor = void(__thiscall*)(INotifyPlayerList* thisptr, char flags);
    Destructor destructor;

    using Method1 = void(__thiscall*)(INotifyPlayerList* thisptr, int a2);
    Method1 method1;
};

assert_vftable_size(INotifyPlayerListVftable, 2);

} // namespace game

#endif // NOTIFYPLAYERLIST_H
