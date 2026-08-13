#ifndef CAPITALSCOUTHOOKS_H
#define CAPITALSCOUTHOOKS_H

#include "capitalscoutapi.h"

namespace hooks {

extern CapitalScoutApi::GetScout originalCapitalGetScout;

int __fastcall capitalGetScoutHooked(const void* fort, int /*%edx*/, const void* objectMap);

} // namespace hooks

#endif
