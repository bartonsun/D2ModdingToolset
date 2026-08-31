#ifndef CAPITALSCOUTAPI_H
#define CAPITALSCOUTAPI_H

#include <cstdint>

namespace hooks {

struct CapitalScoutApi
{
    using GetScout = int(__thiscall*)(const void* fort, const void* objectMap);
    GetScout capitalGetScout;

    std::uintptr_t cityFogSlot8Return;
};

const CapitalScoutApi& capitalScoutApi();

} // namespace hooks

#endif
