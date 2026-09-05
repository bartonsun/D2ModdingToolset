/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/bartonsun/D2ModdingToolset)
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "lobbysaveresume.h"
#include "gameutils.h"
#include "midgardscenariomap.h"
#include "midscenvariables.h"
#include "midserverlogic.h"
#include "midstreamenvfile.h"
#include "netcustomservice.h"
#include "netplayerinfo.h"
#include "scenarioheader.h"
#include "scenarioinfo.h"
#include "version.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <spdlog/spdlog.h>

namespace hooks {
namespace {

constexpr char turnBaseVariable[] = "CONCLAVE_TURN_BASE";

// Native race comparator (Russobit 0x42e849), before moving the original host first.
constexpr std::array<game::RaceId, 6> raceOrder = {
    game::RaceId::Human, game::RaceId::Heretic, game::RaceId::Dwarf,
    game::RaceId::Undead, game::RaceId::Elf, game::RaceId::Neutral,
};

const game::ScenarioVariableData* findTurnBase(const game::CMidScenVariables* variables)
{
    if (variables) {
        for (const auto& variable : variables->variables) {
            if (std::strcmp(variable.second.name, turnBaseVariable) == 0) {
                return &variable.second;
            }
        }
    }
    return nullptr;
}

bool resumeSupported()
{
    // The queue and initial BeginTurn layout are verified on this executable only.
    return gameVersion() == GameVersion::Russobit && CNetCustomService::get();
}

} // namespace

void captureLobbySaveTurnBase(game::CMidgardScenarioMap* scenarioMap,
                              const game::CMidStreamEnvFile* streamEnv)
{
    using namespace game;
    if (!resumeSupported() || !streamEnv->readMode || !streamEnv->fileName.string) {
        return;
    }
    const auto info = getScenarioInfo(scenarioMap);
    const auto variables = getScenarioVariables(scenarioMap);
    if (!info || info->currentTurn <= 0 || !variables || findTurnBase(variables)) {
        return;
    }

    ScenarioFileHeader header{};
    CMidgardID scenarioId{};
    if (!ScenarioFileHeaderApi::get().readAndCheckHeader(
            streamEnv->fileName.string, &scenarioId, &header, nullptr, nullptr, nullptr)
        || scenarioId != scenarioMap->scenarioFileId
        || std::find(raceOrder.begin(), raceOrder.end(), header.race) == raceOrder.end()) {
        spdlog::warn("Lobby resume: cannot read the saved turn-order origin");
        return;
    }

    // This is a native scenario variable, not an extra field in the .sg format. Keep the
    // original origin when another race hosts the next session and creates another save.
    int id = 1;
    for (const auto& variable : variables->variables) {
        if (variable.first == std::numeric_limits<int>::max()) {
            return;
        }
        id = std::max(id, variable.first + 1);
    }
    ScenarioVariable entry{};
    entry.first = id;
    std::memcpy(entry.second.name, turnBaseVariable, sizeof(turnBaseVariable));
    entry.second.value = static_cast<int>(header.race);
    auto writable = static_cast<CMidScenVariables*>(
        scenarioMap->vftable->findScenarioObjectByIdForChange(scenarioMap, &variables->id));
    ScenarioVariableInsertResult result{};
    CMidScenVariablesApi::get().insert(&writable->variables, &result, &entry);
}

bool prepareLobbySaveResume(game::CMidServerLogic* logic)
{
    using namespace game;
    if (!resumeSupported() || !logic->coreData->multiplayerGame || logic->coreData->hotseatGame) {
        return false;
    }
    const auto base = findTurnBase(getScenarioVariables(logic->coreData->objectMap));
    auto players = logic->coreData->players;
    if (!base || !players || !players->bgn || players->bgn == players->end) {
        return false;
    }
    const auto count = players->size();
    const auto offset = logic->coreData->loadedTurnOffset;
    const auto origin = static_cast<RaceId>(base->value);
    if (count > raceOrder.size() || offset < 0 || static_cast<std::size_t>(offset) >= count) {
        spdlog::warn("Lobby resume: invalid queue size/offset ({}/{})", count, offset);
        return false;
    }

    std::array<bool, raceOrder.size()> seen{};
    bool originPresent = false;
    for (auto player = players->bgn; player != players->end; ++player) {
        const auto race = player->raceCategory.id;
        const auto position = std::find(raceOrder.begin(), raceOrder.end(), race);
        if (position == raceOrder.end() || seen[position - raceOrder.begin()]) {
            spdlog::warn("Lobby resume: unknown or duplicate race in native queue");
            return false;
        }
        seen[position - raceOrder.begin()] = true;
        originPresent = originPresent || race == origin;
    }
    if (!originPresent) {
        spdlog::warn("Lobby resume: saved origin is absent from native queue");
        return false;
    }

    // Rebuild the original base, then rotate ALL players, not just the non-host races.
    std::sort(players->bgn, players->end, [origin](const auto& left, const auto& right) {
        const auto rank = [origin](RaceId race) {
            return race == origin ? -1 : static_cast<int>(
                std::find(raceOrder.begin(), raceOrder.end(), race) - raceOrder.begin());
        };
        return rank(left.raceCategory.id) < rank(right.raceCategory.id);
    });
    std::rotate(players->bgn, players->bgn + offset, players->end);
    // Native initialization now starts at index 0; keeping offset preserves the day boundary
    // and the next save's (currentPlayerIndex + loadedTurnOffset) % count calculation.
    spdlog::info("Lobby resume: active race={}, origin={}, offset={}, queue={}",
                 static_cast<int>(players->bgn->raceCategory.id), base->value, offset, count);
    return true;
}

} // namespace hooks
