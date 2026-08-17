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

#ifndef MQPOINTLIST_H
#define MQPOINTLIST_H

#include "mqpoint.h"

namespace game {

struct CMqPoint;
struct CStreamBits;

struct CMqPointListNode
{
    CMqPointListNode* next; /**< +0 */
    CMqPointListNode* prev; /**< +4 */
    CMqPoint position;      /**< +8, 8 bytes */
};

/** Circular doubly-linked list of map positions (LinkedList<CMqPoint> in GOG). */
struct CMqPointList
{
    int unknown;            /**< +0  */
    void* allocator;        /**< +4  */
    CMqPointListNode* head; /**< +8  sentinel */
    int length;             /**< +C  */
};

namespace CMqPointListApi {

struct Api
{
    using Ctor = CMqPointList*(__thiscall*)(CMqPointList* thisptr);
    Ctor ctor;

    /** ImageLayerPairListAdd / LinkedList::add */
    using Add = void(__thiscall*)(CMqPointList* thisptr, const CMqPoint* position);
    Add add;

    /** ImageLayerPairListClear / LinkedList::clear */
    using Clear = void(__thiscall*)(CMqPointList* thisptr);
    Clear clear;

    using Dtor = void(__thiscall*)(CMqPointList* thisptr);
    Dtor dtor;

    using Serialize = bool(__stdcall*)(CStreamBits* stream, CMqPointList* list);
    Serialize serialize;
};

Api& get();

} // namespace CMqPointListApi
} // namespace game

#endif // MQPOINTLIST_H
