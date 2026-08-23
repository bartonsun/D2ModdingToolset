#include "ruinmovement.h"
#include <cassert>

int main()
{
    using namespace hooks;

    assert(shouldHandleLootedRuinMovement(true, true, true));
    assert(!shouldHandleLootedRuinMovement(false, true, true));
    assert(!shouldHandleLootedRuinMovement(true, false, true));
    assert(!shouldHandleLootedRuinMovement(true, true, false));

    assert(pointTargetsRuin(12, 12, 10, 10, 3, 3, 13, 11));
    assert(pointTargetsRuin(13, 11, 10, 10, 3, 3, 13, 11));
    assert(!pointTargetsRuin(13, 12, 10, 10, 3, 3, 13, 11));
    assert(!pointTargetsRuin(10, 10, 10, 10, 0, 3, 13, 11));

    assert(lootedRuinMovementRefund(true, 34, 10, 34, 7) == 17);
    assert(lootedRuinMovementRefund(true, 20, 18, 34, 0) == 2);
    assert(lootedRuinMovementRefund(true, 17, 0, 34, 1) == 16);
    assert(lootedRuinMovementRefund(true, 20, 15, 34, 5) == 0);
    assert(lootedRuinMovementRefund(false, 25, 0, 34, 0) == 0);
    assert(lootedRuinMovementRefund(true, -1, 0, 34, 0) == 0);
    assert(lootedRuinMovementRefund(true, 20, -1, 34, 0) == 0);
    assert(lootedRuinMovementRefund(true, 20, 20, 34, 0) == 0);
    assert(lootedRuinMovementRefund(true, 18, 20, 34, 0) == 0);
    assert(lootedRuinMovementRefund(true, 20, 0, 0, 0) == 0);
    assert(lootedRuinMovementRefund(true, 20, 0, 34, -1) == 0);

    return 0;
}
