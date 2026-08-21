#include "endturnapi.h"
#include "version.h"
#include <array>

namespace game::EndTurnApi {

static std::array<Api, 4> functions = {{
    Api{
        (Api::SendEndTurnMsg)0x406489,
        (Api::StratInterfEndTurn)0x48fdd7,
    },
    Api{
        (Api::SendEndTurnMsg)0x406489,
        (Api::StratInterfEndTurn)0x48fdd7,
    },
    Api{
        (Api::SendEndTurnMsg)0x406115,
        (Api::StratInterfEndTurn)0x48f911,
    },
    Api{
        (Api::SendEndTurnMsg)nullptr,
        (Api::StratInterfEndTurn)nullptr,
    },
}};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::EndTurnApi
