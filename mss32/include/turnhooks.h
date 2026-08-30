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

void armRestoredGameDailyIncomeSuppression();

void clearRestoredGameDailyIncomeSuppression();

bool isRestoredGameDailyIncomeSuppressed();

} // namespace hooks

#endif
