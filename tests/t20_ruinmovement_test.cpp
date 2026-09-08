#include "ruinmovement.h"
#include <cassert>

int main()
{
    using namespace hooks;

    assert(shouldHandleLootedRuinMovement(true, true, true));
    assert(!shouldHandleLootedRuinMovement(false, true, true));
    assert(!shouldHandleLootedRuinMovement(true, false, true));
    assert(!shouldHandleLootedRuinMovement(true, true, false));

    assert(shouldSkipLootedRuinEntry(true, true, true));
    assert(!shouldSkipLootedRuinEntry(false, true, true));
    assert(!shouldSkipLootedRuinEntry(true, false, true));
    assert(!shouldSkipLootedRuinEntry(true, true, false));
    assert(!shouldSkipLootedRuinEntry(false, false, false));

    assert(!skipLootedRuinCallsOriginal(true));
    assert(skipLootedRuinCallsOriginal(false));

    assert(pointTargetsRuin(12, 12, 10, 10, 3, 3, 13, 11));
    assert(pointTargetsRuin(13, 11, 10, 10, 3, 3, 13, 11));
    assert(!pointTargetsRuin(13, 12, 10, 10, 3, 3, 13, 11));
    assert(!pointTargetsRuin(10, 10, 10, 10, 0, 3, 13, 11));

    assert(movementAfterLootedRuinClick(true, 17, 0) == 17);
    assert(movementAfterLootedRuinClick(true, 34, 10) == 34);
    assert(movementAfterLootedRuinClick(true, 20, 18) == 20);
    assert(movementAfterLootedRuinClick(false, 17, 0) == 0);
    assert(movementAfterLootedRuinClick(false, 34, 10) == 10);
    assert(movementAfterLootedRuinClick(false, 20, 18) == 18);
    assert(movementAfterLootedRuinClick(false, 25, 12) == 12);
    assert(movementAfterLootedRuinClick(true, 0, 0) == 0);
    assert(movementAfterLootedRuinClick(true, 8, 8) == 8);

    return 0;
}
