#include "capitalscoutapi.h"
#include "version.h"
#include <array>

namespace hooks {

namespace {

using Api = CapitalScoutApi;

static std::array<Api, 4> functions = {{
    Api{
        (Api::GetScout)0x608250,
        0x5d5eb3,
    },
    Api{
        (Api::GetScout)0x608250,
        0x5d5eb3,
    },
    Api{
        (Api::GetScout)0x606d1a,
        0x5d4ddc,
    },
    Api{
        (Api::GetScout)nullptr,
        0,
    },
}};

} // namespace

const CapitalScoutApi& capitalScoutApi()
{
    return functions[static_cast<int>(gameVersion())];
}

} // namespace hooks
