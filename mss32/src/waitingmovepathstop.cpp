#include "waitingmovepathstop.h"
#include "midstack.h"
#include "waitingmovepath.h"
#include <algorithm>

namespace hooks {

game::CMqPoint findWaitingMovementStop(const game::IMidgardObjectMap* objectMap,
                                      const game::CMidgardPlan* plan,
                                      const game::CMidStack* stack,
                                      game::PathInfoList* pathInfo,
                                      const game::CMqPoint* pathEnd,
                                      int budget,
                                      bool waterOnly,
                                      const game::CMqPoint* previewStart,
                                      game::CMqPoint* resolvedActionTarget,
                                      game::CMqPoint* resolvedPathEnd)
{
    using namespace game;

    const auto* api = WaitingMovementPathApi::get();
    if (!api || !api->findMovementStop || !pathEnd || !pathInfo || !pathInfo->length) {
        return previewStart ? *previewStart : stack->position;
    }

    CMidStack previewStack{*stack};
    previewStack.movement = static_cast<std::uint8_t>(std::clamp(budget, 0, 255));
    if (previewStart) {
        previewStack.position = *previewStart;
        // The first segment has already left its city and reached the battle.
        previewStack.insideId = game::CMidgardID{0};
    }

    const auto& pathApi = PathInfoListApi::get();
    PathInfoList copy{};
    pathApi.constructor(&copy);
    pathApi.populateFromPath(objectMap, &previewStack,
                             reinterpret_cast<const List<CMqPoint>*>(pathInfo), pathEnd,
                             waterOnly, &copy);
    CMqPoint actionTarget = *pathEnd;
    CMqPoint end = *pathEnd;
    // false: CPath does not count invisible stacks (the route mode is already applied).
    const auto stop = api->findMovementStop(objectMap, plan, &previewStack, false,
                                            reinterpret_cast<List<CMqPoint>*>(&copy),
                                            &actionTarget, &end);
    if (resolvedActionTarget) {
        *resolvedActionTarget = actionTarget;
    }
    if (resolvedPathEnd) {
        *resolvedPathEnd = end;
    }
    pathApi.freeNodes(&copy);
    pathApi.freeNode(&copy, copy.head);
    return stop;
}

}
