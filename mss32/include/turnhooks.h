#ifndef TURNHOOKS_H
#define TURNHOOKS_H

#include "phasegame.h"

namespace game {

struct CMidServerLogicData;

struct CMidgardID;

} // namespace game

namespace hooks {

void* getBeginTurnHooked();

void** getBeginTurnOrig();

} // namespace hooks

#endif