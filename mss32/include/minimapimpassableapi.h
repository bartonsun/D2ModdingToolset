#ifndef MINIMAPIMPASSABLEAPI_H
#define MINIMAPIMPASSABLEAPI_H

#include <cstdint>

namespace hooks {

struct MinimapImpassableApi
{
    using LandmarkTypeIsMountain = bool(__thiscall*)(void* landmarkTypeData);
    LandmarkTypeIsMountain landmarkTypeIsMountain;

    std::uintptr_t palmapGateMountainCheckReturn;
};

const MinimapImpassableApi& minimapImpassableApi();

} // namespace hooks

#endif // MINIMAPIMPASSABLEAPI_H
