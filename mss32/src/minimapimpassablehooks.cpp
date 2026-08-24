#include "minimapimpassablehooks.h"
#include "settings.h"
#include <intrin.h>

namespace hooks {

MinimapImpassableApi::LandmarkTypeIsMountain originalLandmarkTypeIsMountain = nullptr;

bool __fastcall landmarkTypeIsMountainHooked(void* landmarkTypeData, int /*%edx*/)
{
    bool mountain = false;
    if (originalLandmarkTypeIsMountain) {
        mountain = originalLandmarkTypeIsMountain(landmarkTypeData);
    }

    if (mountain) {
        return true;
    }

    if (!gameSettings().minimapShowImpassableObjects) {
        return false;
    }

    const auto ret = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (ret == minimapImpassableApi().palmapGateMountainCheckReturn) {
        return true;
    }

    return false;
}

} // namespace hooks
