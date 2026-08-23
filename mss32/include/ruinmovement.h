#ifndef RUINMOVEMENT_H
#define RUINMOVEMENT_H

namespace hooks {

constexpr bool shouldHandleLootedRuinMovement(bool stayOrNear,
                                              bool stackExists,
                                              bool humanMover,
                                              bool lootedRuin)
{
    return stayOrNear && stackExists && humanMover && lootedRuin;
}

constexpr int lootedRuinMovementRefund(bool shouldHandle,
                                       int movementBefore,
                                       int movementAfter)
{
    return shouldHandle && movementBefore >= 0 && movementAfter >= 0
               && movementBefore > movementAfter
        ? movementBefore - movementAfter
        : 0;
}

}

#endif
