/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/Rapthos/Experimental-version)
 * Copyright (C) 2026 Rapthos.
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

#ifndef PLANDATA_H
#define PLANDATA_H

namespace game {

/** Opaque 28-byte accumulator for visitor change records. */
struct PlanDataBuffer
{
    int data[7]{};
};

namespace PlanDataApi {

struct Api
{
    /**
     * Initialises a PlanDataBuffer.
     * sub_419A8F in Akella binary.
     * @param a2 context pointer; nullptr works for simple terrain changes.
     * @param a3 pointer to a zero-initialised int.
     * @param a4 flags; always 0 in observed call sites.
     */
    using Ctor = void*(__thiscall*)(PlanDataBuffer* thisptr, const char* a2, const int* a3, bool a4);
    Ctor ctor;

    /** Destroys a PlanDataBuffer. sub_419AEE in Akella binary. */
    using Dtor = void(__thiscall*)(PlanDataBuffer* thisptr);
    Dtor dtor;
};

Api& get();

} // namespace PlanDataApi
} // namespace game

#endif // PLANDATA_H
