#pragma once

#include "mqpoint.h"

#include <vector>

namespace game {
struct CFortification;
struct CMidgardID;
struct CMidStack;
struct IMidgardObjectMap;
} // namespace game

namespace hooks {

struct WaitingMovementPathNode
{
    game::CMqPoint position;
    int cumulativeCost;
};

using WaitingMovementPath = std::vector<WaitingMovementPathNode>;

bool canEnterWaitingMovementPathFort(const game::IMidgardObjectMap* objectMap,
                                     const game::CMidStack* stack,
                                     const game::CFortification* fort);

bool canExitWaitingMovementPathFort(const game::CMidStack* stack, const game::CFortification* fort);

WaitingMovementPath planWaitingMovementPath(const game::IMidgardObjectMap* objectMap,
                                            const game::CMidStack* stack,
                                            const game::CMqPoint& start,
                                            const game::CMqPoint& destination,
                                            const game::CMidgardID* ignoredStackId = nullptr);

} // namespace hooks
