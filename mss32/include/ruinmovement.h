#ifndef RUINMOVEMENT_H
#define RUINMOVEMENT_H

namespace hooks {

constexpr bool shouldHandleLootedRuinMovement(bool stackExists,
                                              bool humanMover,
                                              bool lootedRuin)
{
    return stackExists && humanMover && lootedRuin;
}

constexpr bool pointTargetsRuin(int pointX,
                                int pointY,
                                int originX,
                                int originY,
                                int sizeX,
                                int sizeY,
                                int entranceX,
                                int entranceY)
{
    if (sizeX <= 0 || sizeY <= 0) {
        return false;
    }
    const bool inside = pointX >= originX && pointX < originX + sizeX && pointY >= originY
        && pointY < originY + sizeY;
    return inside || (pointX == entranceX && pointY == entranceY);
}

constexpr int lootedRuinMovementRefund(bool shouldHandle,
                                       int movementBefore,
                                       int movementAfter,
                                       int maxMovement)
{
    if (!shouldHandle || movementBefore < 0 || movementAfter < 0 || maxMovement <= 0
        || movementBefore <= movementAfter) {
        return 0;
    }
    const int spent = movementBefore - movementAfter;
    const int actionCost = (maxMovement + 1) / 2;
    return spent < actionCost ? spent : actionCost;
}

}

#endif
