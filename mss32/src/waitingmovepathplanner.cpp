#include "waitingmovepathplanner.h"

#include "fortcategory.h"
#include "fortification.h"
#include "game.h"
#include "gameutils.h"
#include "groundcat.h"
#include "midgardmap.h"
#include "midgardmapfog.h"
#include "midgardplan.h"
#include "midplayer.h"
#include "midstack.h"
#include "midunit.h"
#include "midvillage.h"
#include "settings.h"
#include "ussoldier.h"
#include "usstackleader.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <queue>
#include <vector>

namespace hooks {

namespace {

struct MovementTraits
{
    bool leaderAlive;
    bool plainsBonus;
    bool forestBonus;
    bool waterBonus;
    bool waterOnly;
};

struct QueueNode
{
    int cost;
    int index;
};

struct QueueNodeGreater
{
    bool operator()(const QueueNode& left, const QueueNode& right) const
    {
        if (left.cost != right.cost) {
            return left.cost > right.cost;
        }

        return left.index > right.index;
    }
};

constexpr std::array<game::CMqPoint, 8> neighborOffsets{{
    {-1, -1},
    {0, -1},
    {1, -1},
    {-1, 0},
    {1, 0},
    {-1, 1},
    {0, 1},
    {1, 1},
}};

constexpr std::array<game::IdType, 7> blockingObjectTypes{{
    game::IdType::Fortification,
    game::IdType::Landmark,
    game::IdType::Site,
    game::IdType::Ruin,
    game::IdType::Tomb,
    game::IdType::Rod,
    game::IdType::Crystal,
}};

bool isInsideMap(const game::CMqPoint& position, int mapSize)
{
    return position.x >= 0 && position.x < mapSize && position.y >= 0 && position.y < mapSize;
}

bool isVisible(const game::CMidgardMapFog* fog, const game::CMqPoint& position)
{
    bool fogged{true};
    return game::CMidgardMapFogApi::get().getFog(fog, &fogged, &position) && !fogged;
}

bool isAreaVisible(const game::CMidgardMapFog* fog, const game::CMqPoint& position, int mapSize)
{
    for (int y = position.y - 1; y <= position.y + 1; ++y) {
        for (int x = position.x - 1; x <= position.x + 1; ++x) {
            const game::CMqPoint neighbor{x, y};
            if (!isInsideMap(neighbor, mapSize) || !isVisible(fog, neighbor)) {
                return false;
            }
        }
    }

    return true;
}

std::optional<MovementTraits> getMovementTraits(const game::IMidgardObjectMap* objectMap,
                                                const game::CMidStack* stack)
{
    const auto& fn = game::gameFunctions();
    const auto* leader = fn.findUnitById(objectMap, &stack->leaderId);
    if (!leader || !leader->unitImpl) {
        return std::nullopt;
    }

    const auto* stackLeader = fn.castUnitImplToStackLeader(leader->unitImpl);
    const auto* soldier = fn.castUnitImplToSoldier(leader->unitImpl);
    if (!stackLeader || !soldier) {
        return std::nullopt;
    }

    const auto& ground = game::GroundCategories::get();
    return MovementTraits{
        stack->leaderAlive,
        stackLeader->vftable->hasMovementBonus(stackLeader, ground.plain),
        stackLeader->vftable->hasMovementBonus(stackLeader, ground.forest),
        stackLeader->vftable->hasMovementBonus(stackLeader, ground.water),
        soldier->vftable->getWaterOnly(soldier),
    };
}

bool canEnterFort(const game::IMidgardObjectMap* objectMap,
                  const game::CMidStack* stack,
                  const game::CFortification* fort)
{
    const bool occupiedByStack = fort && fort->stackId == stack->id && stack->insideId == fort->id;
    if (!fort || fort->ownerId != stack->ownerId || fort->subraceId != stack->subraceId
        || (fort->stackId != game::emptyId && !occupiedByStack)) {
        return false;
    }

    const auto* vftable = static_cast<const game::CFortificationVftable*>(fort->vftable);
    const auto* category = vftable ? vftable->getCategory(fort) : nullptr;
    if (!category) {
        return false;
    }

    const auto& categories = game::FortCategories::get();
    if (category->id == categories.village->id) {
        return static_cast<const game::CMidVillage*>(fort)->riotTurn <= 0;
    }

    if (category->id == categories.capital->id) {
        const auto* player = getPlayer(objectMap, &fort->ownerId);
        return player && player->capturedById == game::emptyId;
    }

    return false;
}

bool canExitFort(const game::CMidStack* stack, const game::CFortification* fort)
{
    if (!fort || stack->insideId != fort->id || fort->stackId != stack->id) {
        return false;
    }

    const auto* vftable = static_cast<const game::CFortificationVftable*>(fort->vftable);
    const auto* category = vftable ? vftable->getCategory(fort) : nullptr;
    if (!category) {
        return false;
    }

    const auto& categories = game::FortCategories::get();
    if (category->id == categories.village->id) {
        return static_cast<const game::CMidVillage*>(fort)->riotTurn <= 0;
    }

    return category->id == categories.capital->id;
}

std::optional<int> getTileMovementCost(const game::IMidgardObjectMap* objectMap,
                                       const game::CMidgardMap* map,
                                       const game::CMidgardPlan* plan,
                                       const game::CMidgardMapFog* fog,
                                       const game::CMidStack* stack,
                                       const game::CMidgardID* ignoredStackId,
                                       const game::CMqPoint& position,
                                       const MovementTraits& traits)
{
    if (!isVisible(fog, position)) {
        return std::nullopt;
    }

    const auto& planApi = game::CMidgardPlanApi::get();
    if (planApi.isPositionContainsObjects(plan, &position, blockingObjectTypes.data(),
                                          blockingObjectTypes.size())) {
        return std::nullopt;
    }

    const game::IdType stackType = game::IdType::Stack;
    const auto* stackAtPosition = planApi.getObjectId(plan, &position, &stackType);
    if (stackAtPosition && *stackAtPosition != stack->id
        && (!ignoredStackId || *stackAtPosition != *ignoredStackId)) {
        const auto* blockingStack = getStack(objectMap, stackAtPosition);
        if (!blockingStack || !blockingStack->invisible
            || blockingStack->ownerId == stack->ownerId) {
            return std::nullopt;
        }
    }

    game::LGroundCategory ground{};
    if (!game::CMidgardMapApi::get().getGround(map, &ground, &position, objectMap)) {
        return std::nullopt;
    }

    const game::IdType roadType = game::IdType::Road;
    const bool road = planApi.getObjectId(plan, &position, &roadType) != nullptr;
    const auto& grounds = game::GroundCategories::get();
    const auto& movementCost = gameSettings().movementCost;

    if (ground.id == grounds.water->id) {
        if (!traits.waterOnly) {
            if (!traits.leaderAlive) {
                return movementCost.water.deadLeader;
            }

            return traits.waterBonus ? movementCost.water.withBonus : movementCost.water.dflt;
        }

        if (!isAreaVisible(fog, position, map->mapSize)
            || !game::gameFunctions().isWaterTileSurroundedByWater(&position, objectMap)) {
            return std::nullopt;
        }

        return movementCost.water.waterOnly;
    }

    if (ground.id == grounds.forest->id) {
        if (traits.waterOnly) {
            return std::nullopt;
        }

        if (!traits.leaderAlive) {
            return movementCost.forest.deadLeader;
        }

        return traits.forestBonus ? movementCost.forest.withBonus : movementCost.forest.dflt;
    }

    if (ground.id == grounds.plain->id) {
        if (traits.waterOnly) {
            return std::nullopt;
        }

        if (!traits.leaderAlive) {
            return movementCost.plain.deadLeader;
        }

        if (!traits.plainsBonus && road) {
            return movementCost.plain.onRoad;
        }

        return movementCost.plain.dflt;
    }

    return std::nullopt;
}

} // namespace

bool canEnterWaitingMovementPathFort(const game::IMidgardObjectMap* objectMap,
                                     const game::CMidStack* stack,
                                     const game::CFortification* fort)
{
    return canEnterFort(objectMap, stack, fort);
}

bool canExitWaitingMovementPathFort(const game::CMidStack* stack, const game::CFortification* fort)
{
    return canExitFort(stack, fort);
}

WaitingMovementPath planWaitingMovementPath(const game::IMidgardObjectMap* objectMap,
                                            const game::CMidStack* stack,
                                            const game::CMqPoint& start,
                                            const game::CMqPoint& destination,
                                            const game::CMidgardID* ignoredStackId)
{
    WaitingMovementPath result;
    if (!objectMap || !stack) {
        return result;
    }

    const auto* map = getMidgardMap(objectMap);
    const auto* plan = getMidgardPlan(objectMap);
    const auto* player = getPlayer(objectMap, &stack->ownerId);
    const auto* fog = player ? getFog(objectMap, player) : nullptr;
    const auto traits = getMovementTraits(objectMap, stack);
    if (!map || !plan || !fog || !traits || map->mapSize <= 0 || !isInsideMap(start, map->mapSize)
        || !isInsideMap(destination, map->mapSize) || !isVisible(fog, start)
        || !isVisible(fog, destination)) {
        return result;
    }

    const int mapSize = map->mapSize;
    const int startIndex = start.y * mapSize + start.x;
    const int destinationIndex = destination.y * mapSize + destination.x;
    const std::size_t cellsTotal = static_cast<std::size_t>(mapSize) * mapSize;
    const int infinity = std::numeric_limits<int>::max();
    const int unknownTileCost = -2;
    const int blockedTileCost = -1;

    std::vector<int> bestCost(cellsTotal, infinity);
    std::vector<int> previous(cellsTotal, -1);
    std::vector<int> tileCost(cellsTotal, unknownTileCost);
    std::vector<std::vector<int>> fortExits(cellsTotal);
    std::priority_queue<QueueNode, std::vector<QueueNode>, QueueNodeGreater> queue;

    const auto getCachedTileCost = [&](const game::CMqPoint& position) {
        const int index = position.y * mapSize + position.x;
        if (tileCost[index] == unknownTileCost) {
            const auto cost = getTileMovementCost(objectMap, map, plan, fog, stack, ignoredStackId,
                                                  position, *traits);
            tileCost[index] = cost ? *cost : blockedTileCost;
        }

        return tileCost[index];
    };

    forEachScenarioObject(objectMap, game::IdType::Fortification,
                          [&](const game::IMidScenarioObject* object) {
                              const auto* fort = static_cast<const game::CFortification*>(object);
                              const bool enterable = canEnterFort(objectMap, stack, fort);
                              const bool startsInside = canExitFort(stack, fort);
                              if (!enterable && !startsInside) {
                                  return;
                              }

                              const auto entrance = getObjectEntrance(fort->mapElement.position,
                                                                      fort->mapElement.sizeX,
                                                                      fort->mapElement.sizeY);
                              if (!isInsideMap(entrance, mapSize) || !isVisible(fog, entrance)) {
                                  return;
                              }

                              std::array<int, neighborOffsets.size()> exits{};
                              std::size_t exitsTotal{};
                              for (const auto& offset : neighborOffsets) {
                                  const auto exit = entrance + offset;
                                  if (!isInsideMap(exit, mapSize) || getCachedTileCost(exit) < 0) {
                                      continue;
                                  }

                                  exits[exitsTotal++] = exit.y * mapSize + exit.x;
                              }

                              if (enterable) {
                                  for (std::size_t i = 0; i < exitsTotal; ++i) {
                                      for (std::size_t j = 0; j < exitsTotal; ++j) {
                                          if (i != j) {
                                              fortExits[exits[i]].push_back(exits[j]);
                                          }
                                      }
                                  }
                              }

                              if (startsInside && start == entrance) {
                                  for (std::size_t i = 0; i < exitsTotal; ++i) {
                                      fortExits[startIndex].push_back(exits[i]);
                                  }
                              }
                          });

    bestCost[startIndex] = 0;
    queue.push(QueueNode{0, startIndex});

    while (!queue.empty()) {
        const QueueNode current = queue.top();
        queue.pop();

        if (current.cost != bestCost[current.index]) {
            continue;
        }

        if (current.index == destinationIndex) {
            break;
        }

        const game::CMqPoint currentPosition{current.index % mapSize, current.index / mapSize};
        for (const auto& offset : neighborOffsets) {
            const game::CMqPoint nextPosition = currentPosition + offset;
            if (!isInsideMap(nextPosition, mapSize)) {
                continue;
            }

            const int nextIndex = nextPosition.y * mapSize + nextPosition.x;
            const int stepCost = getCachedTileCost(nextPosition);
            if (stepCost < 0 || current.cost > infinity - stepCost) {
                continue;
            }

            const int nextCost = current.cost + stepCost;
            if (nextCost >= bestCost[nextIndex]) {
                continue;
            }

            bestCost[nextIndex] = nextCost;
            previous[nextIndex] = current.index;
            queue.push(QueueNode{nextCost, nextIndex});
        }

        for (const int nextIndex : fortExits[current.index]) {
            if (current.cost >= bestCost[nextIndex]) {
                continue;
            }

            bestCost[nextIndex] = current.cost;
            previous[nextIndex] = current.index;
            queue.push(QueueNode{current.cost, nextIndex});
        }
    }

    if (destinationIndex != startIndex && previous[destinationIndex] < 0) {
        return result;
    }

    for (int index = destinationIndex;; index = previous[index]) {
        result.push_back(WaitingMovementPathNode{
            game::CMqPoint{index % mapSize, index / mapSize},
            bestCost[index],
        });

        if (index == startIndex) {
            break;
        }
    }

    std::reverse(result.begin(), result.end());
    return result;
}

} // namespace hooks
