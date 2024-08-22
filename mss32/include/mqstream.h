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

#ifndef MQSTREAM_H
#define MQSTREAM_H

#include "d2assert.h"
#include <cstdint>

namespace game {

struct CMqStreamVftable;

struct CMqStream
{
    const CMqStreamVftable* vftable;
    bool read;
    char padding[3];
};

assert_size(CMqStream, 8);

struct CMqStreamVftable
{
    using Destructor = void(__thiscall*)(CMqStream* thisptr, char flags);
    Destructor destructor;

    using Serialize = void(__thiscall*)(CMqStream* thisptr, const void* data, int count);
    Serialize serialize;

    using Method2 = int(__thiscall*)(CMqStream* thisptr);
    Method2 method2;

    using GetNumBytes = std::uint32_t(__thiscall*)(CMqStream* thisptr);
    GetNumBytes getNumBytes;

    using GetBuffer = void*(__thiscall*)(CMqStream* thisptr);
    GetBuffer getBuffer;
};

assert_vftable_size(CMqStreamVftable, 5);

} // namespace game

#endif // MQSTREAM_H
