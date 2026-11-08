/*
 * turnhooks.cpp
 */

#include "turnhooks.h"

#include "commandmsg.h"
#include "game.h"
#include "gameutils.h"
#include "midplayer.h"
#include "phasegame.h"
#include "playerincomehooks.h"
#include "playerview.h"
#include "scripts.h"
#include "midserverlogic.h"

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace hooks {

static game::MidServerLogicDataBeginTurn beginTurnOrig;

static std::optional<sol::environment> env;
// protected_function, not sol::function: under NDEBUG sol::function is
// unsafe_function, and a Lua error inside one leaves the stack unwound past
// this try/catch -- the day-2 crash T-32 traced to exactly this shape.
static std::optional<sol::protected_function> processTurnStart;

bool __fastcall beginTurnHooked(game::CMidServerLogicData* thisptr,
                                int /*%edx*/,
                                game::CMidgardID* playerId)
{
    using namespace game;

    const auto* logic = thisptr ? thisptr->logic : nullptr;
    const TurnIncomeScope incomeScope{logic};
    rememberHumanPlayers(logic);
    const bool credited = isIncomeCredited(logic, playerId);
    const bool result = beginTurnOrig(thisptr, playerId);

    // A repeated turn start (reconnect, restored game) does not run turn.lua again.
    if (!result || !logic || !playerId || credited || !claimTurnStart(logic, playerId)) {
        return result;
    }

    auto objectMap = logic->coreData ? logic->coreData->objectMap : nullptr;

    if (!objectMap) {
        spdlog::error("[TURN] objectMap == nullptr");
        return result;
    }

    if (!processTurnStart) {

        static const auto path = scriptsFolder() / "turn.lua";

        if (!env) {
            env = executeScriptFile(path, false, true);
        }
        if (env) {
            processTurnStart = getProtectedScriptFunction(env.value(), "processTurnStart", false);
        }

        if (!processTurnStart) {

            spdlog::error("[TURN] failed to load processTurnStart");

            return result;
        }
    }

    auto playerObj = objectMap->vftable->findScenarioObjectById(objectMap, playerId);

    if (!playerObj) {

        spdlog::error("[TURN] could not find player {}", idToString(playerId));

        return result;
    }

    auto player = static_cast<const CMidPlayer*>(playerObj);

    bindings::PlayerView playerView(player, objectMap);

    try {

        (*processTurnStart)(playerView);

    } catch (const std::exception& e) {

        spdlog::error("[TURN] Lua exception: {}", e.what());

        showErrorMessageBox(fmt::format("Failed to run turn.lua\nReason: {}", e.what()));
    }

    return result;
}

void* getBeginTurnHooked()
{
    return (void*)beginTurnHooked;
}

void** getBeginTurnOrig()
{
    return (void**)&beginTurnOrig;
}

} // namespace hooks