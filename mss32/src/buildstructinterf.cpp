#include "buildstructinterf.h"
#include "version.h"
#include <array>

namespace game::CBuildStructInterfApi {

static std::array<Api, 3> functions = {{
    Api{(Api::UpdateBuildingInfo)0x4abf2e},
    Api{(Api::UpdateBuildingInfo)0x4abf2e},
    Api{(Api::UpdateBuildingInfo)0x4ab5cc},
}};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CBuildStructInterfApi
