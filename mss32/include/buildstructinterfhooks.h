#ifndef BUILDSTRUCTINTERFHOOKS_H
#define BUILDSTRUCTINTERFHOOKS_H

namespace game {
struct CBuildStructInterf;
}

namespace hooks {

void __fastcall buildStructInterfUpdateBuildingInfoHooked(game::CBuildStructInterf* thisptr,
                                                          int /*%edx*/);

} // namespace hooks

#endif // BUILDSTRUCTINTERFHOOKS_H
