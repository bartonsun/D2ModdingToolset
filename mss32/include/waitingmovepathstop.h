#pragma once

#include "mqpoint.h"
#include "pathinfolist.h"

namespace game {
struct CMidgardPlan;
}

namespace hooks {

game::CMqPoint findWaitingMovementStop(const game::IMidgardObjectMap* objectMap,
                                      const game::CMidgardPlan* plan,
                                      const game::CMidStack* stack,
                                      game::PathInfoList* pathInfo,
                                      const game::CMqPoint* pathEnd,
                                      int budget,
                                      bool waterOnly,
                                      const game::CMqPoint* previewStart = nullptr,
                                      game::CMqPoint* resolvedActionTarget = nullptr,
                                      game::CMqPoint* resolvedPathEnd = nullptr);

}
