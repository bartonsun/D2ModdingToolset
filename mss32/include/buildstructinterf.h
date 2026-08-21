#ifndef BUILDSTRUCTINTERF_H
#define BUILDSTRUCTINTERF_H

#include "d2assert.h"
#include "midgardid.h"
#include "mqrect.h"
#include <cstddef>

namespace game {

struct CDialogInterf;
struct CBuildingBranch;
struct TUsUnitImpl;

struct CBuildStructInterfData
{
    CMidgardID buildingId;
    CBuildingBranch* branch;
    void* unknown8;
    CMqRect frameArea;
    const TUsUnitImpl* displayedUnit;
    void* unknown20;
};

assert_size(CBuildStructInterfData, 36);
assert_offset(CBuildStructInterfData, displayedUnit, 28);

struct CBuildStructInterf
{
    void* vftable;
    void* interfaceData;
    void* taskManagerHolderVftable;
    void* fullScreenInterfData;
    CDialogInterf* dialog;
    char unknown[12];
    void* notifyVftable;
    CBuildStructInterfData* data;
};

assert_offset(CBuildStructInterf, dialog, 16);
assert_offset(CBuildStructInterf, data, 36);

namespace CBuildStructInterfApi {

struct Api
{
    using UpdateBuildingInfo = void(__thiscall*)(CBuildStructInterf* thisptr);
    UpdateBuildingInfo updateBuildingInfo;
};

Api& get();

} // namespace CBuildStructInterfApi

} // namespace game

#endif // BUILDSTRUCTINTERF_H
