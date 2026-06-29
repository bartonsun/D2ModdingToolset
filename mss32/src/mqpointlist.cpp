#include "mqpointlist.h"
#include "version.h"
#include <array>

namespace game::CMqPointListApi {

// clang-format off
std::array<Api, 4> functions = {{
    // Akella
    Api{
        (Api::Ctor)0x41bef1,
        (Api::Add)0x522151,
        (Api::Clear)0x522289,
        (Api::Dtor)0x41636f,
        (Api::Serialize)0x47eaa8,
    },
    // Russobit
    Api{
        (Api::Ctor)0x41bef1,
        (Api::Add)0x522151,
        (Api::Clear)0x522289,
        (Api::Dtor)0x41636f,
        (Api::Serialize)0x47eaa8,
    },
    // Gog
    Api{
        (Api::Ctor)0x41762c,
        (Api::Add)0x409EF4,
        (Api::Clear)0x42C2C1,
        (Api::Dtor)0x417665 ,
        (Api::Serialize)0x47E672,
    },
    // Scenario Editor
    Api{
        (Api::Ctor)0,
        (Api::Add)0,
        (Api::Clear)0,
        (Api::Dtor)0,
        (Api::Serialize)0,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::PositionListApi
