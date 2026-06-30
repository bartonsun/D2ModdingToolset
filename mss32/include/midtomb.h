#ifndef MIDTOMB_H
#define MIDTOMB_H

#include "idlist.h"
#include "mapelement.h"
#include "midgardid.h"
#include "midscenarioobject.h"
#include <cstddef>

namespace game {

struct CMidTomb
    : public IMapElement
    , public IMidScenarioObject
{
    CMidgardID stackOwner; // +1C
    CMidgardID killer;     // +20
    int turn;              // +24
    IdList unitIds;        // +28
};

assert_size(CMidTomb, 56);

namespace CMidTombApi {

struct Api
{
    using Constructor = CMidTomb*(__thiscall*)(CMidTomb* thisptr, const CMidgardID* tombId);
    Constructor constructor;

    using Destructor = void(__thiscall*)(CMidTomb* thisptr);
    Destructor destructor;
};

Api& get();

} // namespace CMidTombApi

} // namespace game

#endif // MIDTOMB_H
