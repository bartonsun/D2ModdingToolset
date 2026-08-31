#include "minimapimpassableapi.h"
#include "version.h"
#include <array>

namespace hooks {

namespace {

using Api = MinimapImpassableApi;

static std::array<Api, 4> functions = {{
    Api{
        (Api::LandmarkTypeIsMountain)0x5992ba,
        0x5cc5fc,
    },
    Api{
        (Api::LandmarkTypeIsMountain)0x5992ba,
        0x5cc5fc,
    },
    Api{
        (Api::LandmarkTypeIsMountain)0x59842f,
        0x5cb518,
    },
    Api{
        (Api::LandmarkTypeIsMountain)nullptr,
        0,
    },
}};

} // namespace

const MinimapImpassableApi& minimapImpassableApi()
{
    return functions[static_cast<int>(gameVersion())];
}

} // namespace hooks
