#include "midtomb.h"
#include "version.h"
#include <array>

namespace game::CMidTombApi {

// clang-format off
static std::array<Api, 3> functions = {{
    // Akella
    Api{
        (Api::Constructor)0x606B7B,
        (Api::Destructor)0x606C0E,
    },
    // Russobit
    Api{
        (Api::Constructor)0x606B7B,
        (Api::Destructor)0x606C0E,
    },
    // GoG
    Api{
        (Api::Constructor)0x605665,
        (Api::Destructor)0x6056F8,
    },
}};
// clang-format on

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CMidTombApi
