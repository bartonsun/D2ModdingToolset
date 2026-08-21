/*
 * turnhooks.cpp
 */

#include "turnhooks.h"

#include "commandmsg.h"
#include "game.h"
#include "gameutils.h"
#include "midplayer.h"
#include "phasegame.h"
#include "phasegamehooks.h"
#include "playerview.h"
#include "scripts.h"
#include "midserverlogic.h"

#include <sol/sol.hpp>
#include <spdlog/spdlog.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace hooks {

using BeginTurnFunc = void(__thiscall*)(game::CMidServerLogicData* thisptr,
                                        game::CMidgardID* playerId);

static BeginTurnFunc beginTurnOrig;

static std::optional<sol::environment> env;
static std::optional<sol::protected_function> processTurnStart;

void __fastcall beginTurnHooked(game::CMidServerLogicData* thisptr,
                                int /*%edx*/,
                                game::CMidgardID* playerId)
{
    using namespace game;

    clearLeftoverRestore();

    if (playerId) {
        spdlog::info("[TURN] pre player={} t={}", idToString(playerId), GetTickCount());
    }

    if (beginTurnOrig) {
        beginTurnOrig(thisptr, playerId);
    }

    if (!thisptr || !playerId) {
        return;
    }

    spdlog::info("[TURN] begin player={} t={}", idToString(playerId), GetTickCount());

    auto objectMap = getServerObjectMap();

    if (!objectMap || !objectMap->vftable || !objectMap->vftable->findScenarioObjectById) {
        spdlog::error("[TURN] objectMap == nullptr");
        return;
    }

    if (!processTurnStart) {

        static const auto path = scriptsFolder() / "turn.lua";

        env = executeScriptFile(path, false, true);
        if (env) {
            processTurnStart = getProtectedScriptFunction(*env, "processTurnStart", false);
        }

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
        if (processTurnStart && processTurnStart->valid()) {
            sol::protected_function_result result = (*processTurnStart)(playerView);
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("[TURN] Lua error: {}", err.what());
            }
        }
    } catch (const std::exception& e) {

        spdlog::error("[TURN] Lua exception: {}", e.what());

        showErrorMessageBox(fmt::format("Failed to run turn.lua\nReason: {}", e.what()));
    } catch (...) {
        spdlog::error("[TURN] Lua unknown exception");
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

} // namespace hooks