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
