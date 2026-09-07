#ifndef BUILDSTRUCTINTERFHOOKS_H
#define BUILDSTRUCTINTERFHOOKS_H

namespace game {
struct CBuildStructInterf;
struct CDialogInterf;
struct CMidgardID;
struct IMidgardObjectMap;
struct TBuildingType;
}

namespace hooks {

void __fastcall buildStructInterfUpdateBuildingInfoHooked(game::CBuildStructInterf* thisptr,
                                                          int /*%edx*/);

void __stdcall buildStructInterfUpdateBuildingPopupHooked(
    const game::IMidgardObjectMap* objectMap,
    const game::CMidgardID* playerId,
    const game::TBuildingType* building,
    game::CDialogInterf* dialog);

} // namespace hooks

#endif // BUILDSTRUCTINTERFHOOKS_H
