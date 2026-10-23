#pragma once

#include <cstddef>
#include "midgardplan.h"
#include <vector>

namespace hooks {

// Copy of the plan as it will be after the battle, for the native stop finder.
class WaitingMovementPlan
{
public:
    template <typename KeepElement>
    WaitingMovementPlan(const game::CMidgardPlan& source, KeepElement keepElement)
        : plan{source}
    {
        for (auto* element = source.dynamicElements.bgn;
             element != source.dynamicElements.end; ++element) {
            if (keepElement(*element)) {
                elements.push_back(*element);
            }
        }
        auto* begin = elements.empty() ? nullptr : elements.data();
        auto* end = begin ? begin + elements.size() : nullptr;
        plan.dynamicElements = {begin, end, end, nullptr};
        for (auto& flags : plan.elementFlags) {
            flags &= 0xaa; // Preserve static occupancy only.
        }
        for (const auto& element : elements) {
            if (element.x < 144 && element.y < 144) {
                plan.elementFlags[36 * element.y + element.x / 4]
                    |= 1 << (2 * (element.x & 3));
            }
        }
    }

    WaitingMovementPlan(const WaitingMovementPlan&) = delete;
    WaitingMovementPlan& operator=(const WaitingMovementPlan&) = delete;

    const game::CMidgardPlan* get() const { return &plan; }

private:
    game::CMidgardPlan plan;
    std::vector<game::MidgardPlanElement> elements;
};

} // namespace hooks
