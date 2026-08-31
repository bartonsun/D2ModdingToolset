#ifndef MINIMAPIMPASSABLEHOOKS_H
#define MINIMAPIMPASSABLEHOOKS_H

#include "minimapimpassableapi.h"

namespace hooks {

extern MinimapImpassableApi::LandmarkTypeIsMountain originalLandmarkTypeIsMountain;

bool __fastcall landmarkTypeIsMountainHooked(void* landmarkTypeData, int /*%edx*/);

} // namespace hooks

#endif // MINIMAPIMPASSABLEHOOKS_H
