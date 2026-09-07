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
#include "playerincomehooks.h"
#include "scripts.h"
#include "midserverlogic.h"

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace hooks {

static game::MidServerLogicDataBeginTurn beginTurnOrig;

static std::optional<sol::environment> env;
static std::optional<sol::function> processTurnStart;

bool __fastcall beginTurnHooked(game::CMidServerLogicData* thisptr,
                                int /*%edx*/,
                                game::CMidgardID* playerId)
{
    using namespace game;

    const bool alreadyCredited = wasPlayerIncomeCredited(getServerObjectMap(), playerId);
    const bool result = beginTurnOrig(thisptr, playerId);

    if (!result || !thisptr || !playerId || alreadyCredited) {
        return result;
    }

    auto objectMap = getServerObjectMap();

    if (!objectMap) {
        spdlog::error("[TURN] objectMap == nullptr");
        return result;
    }

    if (!processTurnStart) {

        static const auto path = scriptsFolder() / "turn.lua";

        processTurnStart = getScriptFunction(path, "processTurnStart", env, false, true);

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
