#include "ruinmovement.h"
#include <cassert>

int main()
{
    using namespace hooks;

    assert(shouldHandleLootedRuinMovement(true, true, true, true));
    assert(!shouldHandleLootedRuinMovement(false, true, true, true));
    assert(!shouldHandleLootedRuinMovement(true, false, true, true));
    assert(!shouldHandleLootedRuinMovement(true, true, false, true));
    assert(!shouldHandleLootedRuinMovement(true, true, true, false));

    assert(lootedRuinMovementRefund(true, 25, 0) == 25);
    assert(lootedRuinMovementRefund(true, 20, 18) == 2);
    assert(lootedRuinMovementRefund(false, 25, 0) == 0);
    assert(lootedRuinMovementRefund(true, -1, 0) == 0);
    assert(lootedRuinMovementRefund(true, 20, -1) == 0);
    assert(lootedRuinMovementRefund(true, 20, 20) == 0);
    assert(lootedRuinMovementRefund(true, 18, 20) == 0);

    return 0;
}
