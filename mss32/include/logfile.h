/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Stanislav Egorov.
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

#ifndef LOGFILE_H
#define LOGFILE_H

#include "d2assert.h"

namespace game {

struct CLogFileVftable;

struct CLogFile
{
    CLogFileVftable* vftable;
};

struct CLogFileVftable
{
    using Destructor = void(__thiscall*)(CLogFile* thisptr, char flags);
    Destructor destructor;

    using Log = void(__thiscall*)(CLogFile* thisptr, const char* message);
    Log log;
};

assert_vftable_size(CLogFileVftable, 2);

} // namespace game

#endif // LOGFILE_H
