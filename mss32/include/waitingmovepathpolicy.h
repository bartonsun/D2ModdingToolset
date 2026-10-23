#pragma once

namespace hooks {

enum class WaitingPathFlag { Gray, Action, AfterBattle, Battle, Negotiate };

inline WaitingPathFlag waitingPathFlag(bool passedStop,
                                       bool atStop,
                                       bool leadsToAction,
                                       bool noble,
                                       int cost,
                                       int budget)
{
    // A battle can begin at the origin, so atStop wins over passedStop.
    if (passedStop && !atStop) {
        return leadsToAction ? WaitingPathFlag::AfterBattle : WaitingPathFlag::Action;
    }
    if (cost > budget || (atStop && leadsToAction && cost >= budget)) {
        return WaitingPathFlag::Action;
    }
    if (atStop && leadsToAction) {
        return noble ? WaitingPathFlag::Negotiate : WaitingPathFlag::Battle;
    }
    return WaitingPathFlag::Gray;
}

// Movement left on a tile of the route after a battle; on the following days
// it counts down from the full movement again.
inline int waitingPathRemainingMovement(int spent, int budget, int maxMovement)
{
    if (spent <= budget || maxMovement <= 0) {
        return budget > spent ? budget - spent : 0;
    }
    const int over = spent - budget;
    const int days = 1 + (over - 1) / maxMovement;
    return days * maxMovement - over;
}

inline bool waitingPathModeNeedsUpdate(bool destinationValid,
                                        bool modeValid,
                                        int previousRequestedMode,
                                        int requestedMode)
{
    return destinationValid && modeValid && previousRequestedMode != requestedMode;
}

// A rejected destination (fog, blocked tile) keeps the previous route.
inline bool waitingPathUpdateCanKeepPrevious(bool hadPreviousRoute, bool restoredPreviousRoute)
{
    return hadPreviousRoute && restoredPreviousRoute;
}

// CPath's action mask marks endpoints that cannot be passed, e.g. a clicked enemy.
inline bool waitingPathStackTilePassable(bool stackPresent,
                                         bool sameStack,
                                         bool hiddenForeignStack,
                                         bool nativeActionTile)
{
    return !stackPresent || sameStack || hiddenForeignStack || nativeActionTile;
}

} // namespace hooks
