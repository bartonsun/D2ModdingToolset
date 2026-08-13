#include "capitalscouthooks.h"
#include "settings.h"
#include <intrin.h>

namespace hooks {

CapitalScoutApi::GetScout originalCapitalGetScout = nullptr;

int __fastcall capitalGetScoutHooked(const void* fort, int /*%edx*/, const void* objectMap)
{
    const int overrideRange = gameSettings().capitalScoutRange;
    if (overrideRange > 0) {
        const auto ret = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        if (ret == capitalScoutApi().cityFogSlot8Return) {
            return overrideRange;
        }
    }

    if (originalCapitalGetScout) {
        return originalCapitalGetScout(fort, objectMap);
    }

    return 0;
}

} // namespace hooks
