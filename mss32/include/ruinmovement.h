#ifndef RUINMOVEMENT_H
#define RUINMOVEMENT_H

namespace hooks {

constexpr bool shouldHandleLootedRuinMovement(bool stackExists,
                                              bool humanMover,
                                              bool lootedRuin)
{
    return stackExists && humanMover && lootedRuin;
}

constexpr bool shouldSkipLootedRuinEntry(bool stackExists,
                                         bool humanMover,
                                         bool lootedRuin)
{
    return shouldHandleLootedRuinMovement(stackExists, humanMover, lootedRuin);
}

constexpr bool skipLootedRuinCallsOriginal(bool skipEntry)
{
    return !skipEntry;
}

constexpr int movementAfterLootedRuinClick(bool skipEntry, int movementBefore, int originalAfter)
{
    if (!skipEntry) {
        return originalAfter;
    }
    return movementBefore;
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

}

#endif
