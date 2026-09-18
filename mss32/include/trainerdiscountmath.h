/*
 * This file is part of the modding toolset for Disciples 2.
 *
 * The numbers the camp discount is built on.
 *
 * The camp counts how much experience the treasury buys by spending a copy of
 * it one vanilla step price at a time. With a discount the treasury buys more,
 * so the count runs on the gold the discount is worth, and the course is then
 * charged at the discounted price out of the real treasury. A count that
 * stopped because the treasury ran out spends the whole treasury.
 *
 * It lives in a header with no game types so the contract test can compile and
 * run it on the host.
 */

#ifndef TRAINERDISCOUNTMATH_H
#define TRAINERDISCOUNTMATH_H

#include <algorithm>

namespace hooks {
namespace TrainerDiscountMath {

inline int clampedPercent(int lowerCostPercent)
{
    return std::clamp(lowerCostPercent, 1, 99);
}

inline int countingGold(int gold, int lowerCostPercent)
{
    if (gold <= 0) {
        return 0;
    }
    const long long worth = static_cast<long long>(gold) * 100
                            / (100 - clampedPercent(lowerCostPercent));
    return static_cast<int>(std::min<long long>(worth, 9999));
}

inline bool countingCapped(int gold, int lowerCostPercent)
{
    if (gold <= 0) {
        return false;
    }
    return static_cast<long long>(gold) * 100 / (100 - clampedPercent(lowerCostPercent)) > 9999;
}

inline bool bankRanOut(int left, int step)
{
    return step > 0 && left < step;
}

inline int keptGold(int cost, int before, int lowerCostPercent)
{
    if (cost <= 0 || before <= 0) {
        return 0;
    }
    const long long keep = static_cast<long long>(cost)
                           * (100 - clampedPercent(lowerCostPercent)) / 100;
    return static_cast<int>(std::min<long long>(std::max<long long>(keep, 1), before));
}

inline int chargedGold(int cost, int before, int lowerCostPercent, bool ranOut)
{
    if (cost <= 0 || before <= 0) {
        return 0;
    }
    return ranOut ? before : keptGold(cost, before, lowerCostPercent);
}

} // namespace TrainerDiscountMath
} // namespace hooks

#endif // TRAINERDISCOUNTMATH_H
