#include "plandata.h"
#include "version.h"
#include <array>

namespace game::PlanDataApi {

// clang-format off
std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::Ctor)0x419a8f,
        (Api::Dtor)0x419aee,
    },
    // Russobit
    Api{
        (Api::Ctor)0x419a8f,
        (Api::Dtor)0x419aee,
    },
    // Gog
    Api{
        (Api::Ctor)0x50ba21,
        (Api::Dtor)0x50bD7e,
    },
    // Scenario Editor
    Api{
        (Api::Ctor)0, //TODO
        (Api::Dtor)0, //TODO
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::PlanDataApi
