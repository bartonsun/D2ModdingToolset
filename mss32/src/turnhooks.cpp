/*
 * turnhooks.cpp
 */

#include "turnhooks.h"

#include "commandmsg.h"
#include "game.h"
#include "gameutils.h"
#include "midplayer.h"
#include "phasegame.h"
#include "playerview.h"
#include "scripts.h"
#include "midserverlogic.h"

#include <atomic>
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace hooks {

using BeginTurnFunc = void(__thiscall*)(game::CMidServerLogicData* thisptr,
                                        game::CMidgardID* playerId);

static BeginTurnFunc beginTurnOrig;

static std::optional<sol::environment> env;
static std::optional<sol::function> processTurnStart;
static std::atomic_bool restoredGameDailyIncomePending{false};
static thread_local bool restoredGameDailyIncomeSuppressed;

class ScopedRestoredGameDailyIncomeSuppression
{
public:
    ScopedRestoredGameDailyIncomeSuppression()
        : previous{restoredGameDailyIncomeSuppressed}
    {
        restoredGameDailyIncomeSuppressed = true;
    }

    ~ScopedRestoredGameDailyIncomeSuppression()
    {
        restoredGameDailyIncomeSuppressed = previous;
    }

private:
    bool previous;
};

void __fastcall beginTurnHooked(game::CMidServerLogicData* thisptr,
                                int /*%edx*/,
                                game::CMidgardID* playerId)
{
    using namespace game;

    if (restoredGameDailyIncomePending.exchange(false)) {
        ScopedRestoredGameDailyIncomeSuppression suppression;
        beginTurnOrig(thisptr, playerId);
        return;
    }

    beginTurnOrig(thisptr, playerId);

    if (!thisptr || !playerId) {
        return;
    }

    auto objectMap = getServerObjectMap();

    if (!objectMap) {
        spdlog::error("[TURN] objectMap == nullptr");
        return;
    }

    if (!processTurnStart) {

        static const auto path = scriptsFolder() / "turn.lua";

        processTurnStart = getScriptFunction(path, "processTurnStart", env, false, true);

        if (!processTurnStart) {

            spdlog::error("[TURN] failed to load processTurnStart");

            return;
        }
    }

    auto playerObj = objectMap->vftable->findScenarioObjectById(objectMap, playerId);

    if (!playerObj) {

        spdlog::error("[TURN] could not find player {}", idToString(playerId));

        return;
    }

    auto player = static_cast<const CMidPlayer*>(playerObj);

    bindings::PlayerView playerView(player, objectMap);

    try {

        (*processTurnStart)(playerView);

    } catch (const std::exception& e) {

        spdlog::error("[TURN] Lua exception: {}", e.what());

        showErrorMessageBox(fmt::format("Failed to run turn.lua\nReason: {}", e.what()));
    }
}

void* getBeginTurnHooked()
{
    return (void*)beginTurnHooked;
}

void** getBeginTurnOrig()
{
    return (void**)&beginTurnOrig;
}

void armRestoredGameDailyIncomeSuppression()
{
    restoredGameDailyIncomePending = true;
}

void clearRestoredGameDailyIncomeSuppression()
{
    restoredGameDailyIncomePending = false;
}

bool isRestoredGameDailyIncomeSuppressed()
{
    return restoredGameDailyIncomeSuppressed;
}

} // namespace hooks
