/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2026 Alexey Voskresensky.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fortview.h"
#include "game.h"
#include "gameutils.h"
#include "idlist.h"
#include "midgardid.h"
#include "midplayer.h"
#include "phasegame.h"
#include "playerbuildings.h"
#include "racetype.h"
#include "scripts.h"
#include "unitsforhire.h"
#include "usracialsoldier.h"
#include "ussoldier.h"
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace hooks {

static sol::table idListToTable(lua_State* L, const game::IdList* list)
{
    sol::state_view lua(L);
    sol::table result = lua.create_table();
    for (const auto& id : *list) {
        result.add(bindings::IdView(&id));
    }
    return result;
}

static void tableToIdList(const sol::table& table, game::IdList* list)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& listApi = IdListApi::get();
    const auto& idApi = CMidgardIDApi::get();
    const auto& global = GlobalDataApi::get();
    const auto globalData = *global.getGlobalData();

    listApi.clear(list);

    for (auto& kv : table) {
        sol::object obj = kv.second;
        if (!obj.is<bindings::IdView>()) {
            spdlog::debug("Skipping non-IdView value in Lua table");
            continue;
        }

        auto idView = obj.as<bindings::IdView>();
        const CMidgardID& id = idView;

        auto unitImpl = static_cast<TUsUnitImpl*>(global.findById(globalData->units, &id));
        if (!unitImpl) {
            spdlog::debug("Skipping non-existent unit ID: {}", hooks::idToString(&id));
            continue;
        }

        auto soldier = fn.castUnitImplToRacialSoldier(unitImpl);
        if (!soldier) {
            spdlog::debug("Skipping non-soldier unit ID: {}", hooks::idToString(&id));
            continue;
        }

        listApi.pushBack(list, &id);
    }
}

void __stdcall addUnitsToHireList(game::TRaceType* race,
                                  const game::CPlayerBuildings* playerBuildings,
                                  game::IdList* hireList)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& id = CMidgardIDApi::get();
    const auto& list = IdListApi::get();
    const auto& global = GlobalDataApi::get();
    const auto globalData = *global.getGlobalData();
    const auto& unitBranch = UnitBranchCategories::get();

    fn.addUnitToHireList(race, playerBuildings, unitBranch.fighter, hireList);
    fn.addUnitToHireList(race, playerBuildings, unitBranch.archer, hireList);
    fn.addUnitToHireList(race, playerBuildings, unitBranch.mage, hireList);
    fn.addUnitToHireList(race, playerBuildings, unitBranch.special, hireList);

    game::CMidgardID sideshowId;
    fn.getSideshowUnitImpl(race, &sideshowId, unitBranch.sideshow);

    auto unitImpl = static_cast<TUsUnitImpl*>(global.findById(globalData->units, &sideshowId));
    if (unitImpl) {
        auto soldier = fn.castUnitImplToRacialSoldier(unitImpl);
        if (soldier) {
            list.pushBack(hireList, &sideshowId);
        }
    }
   
    if (!unitsForHire().empty()) {
        const int raceIndex = id.getTypeIndex(&race->id);

        const auto& units = unitsForHire()[raceIndex];
        for (const auto& unit : units) {
            list.pushBack(hireList, &unit);
        }
    }
}

bool __stdcall addPlayerUnitsToHireListHooked(game::CMidDataCache2* dataCache,
                                              const game::CMidgardID* playerId,
                                              const game::CMidgardID* fortificationId,
                                              game::IdList* hireList)
{
    using namespace game;

    const auto& list = IdListApi::get();
    list.clear(hireList);

    const auto& id = CMidgardIDApi::get();
    if (id.getType(fortificationId) != IdType::Fortification) {
        return false;
    }

    auto player = getPlayer(dataCache, playerId);
    if (!player) {
        spdlog::error("Could not find player object with id {:x}", playerId->value);
        return false;
    }

    auto playerBuildings = getPlayerBuildings(dataCache, player);
    if (!playerBuildings) {
        spdlog::error("Could not find player buildings with id {:x}", player->buildingsId.value);
        return false;
    }

    const auto& global = GlobalDataApi::get();
    const auto globalData = *global.getGlobalData();
    game::RacesMap** races = globalData->races;
    TRaceType* race = (TRaceType*)global.findById(races, &player->raceId);

    const auto& fn = gameFunctions();
    auto variables = getScenarioVariables(dataCache);

    const auto& racesCat = RaceCategories::get();
    const auto raceId = player->raceType->data->raceType.id;
    const char* racePrefix{};
    if (raceId == racesCat.human->id) {
        racePrefix = "EMPIRE_";
    } else if (raceId == racesCat.heretic->id) {
        racePrefix = "LEGIONS_";
    } else if (raceId == racesCat.dwarf->id) {
        racePrefix = "CLANS_";
    } else if (raceId == racesCat.undead->id) {
        racePrefix = "HORDES_";
    } else if (raceId == racesCat.elf->id) {
        racePrefix = "ELVES_";
    }

    bool hireAnyRace{false};
    if (variables) {
        for (const auto& variable : variables->variables) {
            const auto expectedName{"HIRE_UNIT_ANY_RACE"};
            const auto expectedNameRace{fmt::format("{:s}HIRE_UNIT_ANY_RACE", racePrefix)};
            const auto& name = variable.second.name;
            if (!strncmp(name, expectedName, sizeof(name))) {
                hireAnyRace = (bool)variable.second.value;
            } else if (!strncmp(name, expectedNameRace.c_str(), sizeof(name))) {
                hireAnyRace = (bool)variable.second.value;
                break;
            }
        }
    }

    if (hireAnyRace) {
        for (size_t i = 0; races[i] != nullptr; ++i) {
            RacesMap* currentMap = races[i];
            Vector<Pair<CMidgardID, TRaceType*>>& dataVec = currentMap->data;
            for (auto* current = dataVec.bgn; current != dataVec.end; ++current) {
                addUnitsToHireList(current->second, playerBuildings, hireList);
            }
        }
    } else {
        addUnitsToHireList(race, playerBuildings, hireList);
    }

    const auto& buildList = playerBuildings->buildings;
    if (buildList.length != 0 && variables && variables->variables.length) {
        int hireTierMax{0};
        for (const auto& variable : variables->variables) {
            const auto expectedNameLegacy{"UNIT_HIRE_TIER_MAX"};
            const auto expectedName{"HIRE_UNIT_TIER_MAX"};
            const auto expectedNameRace{fmt::format("{:s}HIRE_UNIT_TIER_MAX", racePrefix)};
            const auto& name = variable.second.name;
            if (!strncmp(name, expectedNameLegacy, sizeof(name))) {
                hireTierMax = variable.second.value;
            } else if (!strncmp(name, expectedName, sizeof(name))) {
                hireTierMax = variable.second.value;
            } else if (!strncmp(name, expectedNameRace.c_str(), sizeof(name))) {
                hireTierMax = variable.second.value;
                break;
            }
        }

        if (hireTierMax > 1) {
            const auto units = globalData->units;
            for (auto current = units->map->data.bgn, end = units->map->data.end; current != end;
                 ++current) {
                const auto unitImpl = current->second;
                auto soldier = fn.castUnitImplToSoldier(unitImpl);
                if (!soldier)
                    continue;

                if (!hireAnyRace && race->id != *soldier->vftable->getRaceId(soldier)) {
                    continue;
                }

                auto racialSoldier = fn.castUnitImplToRacialSoldier(unitImpl);
                if (!racialSoldier)
                    continue;

                auto upgradeBuildingId = racialSoldier->vftable->getUpgradeBuildingId(
                    racialSoldier);
                if (*upgradeBuildingId == emptyId)
                    continue;

                bool hasBuilding{false};
                for (auto node = buildList.head->next; node != buildList.head; node = node->next) {
                    if (node->data == *upgradeBuildingId) {
                        hasBuilding = true;
                        break;
                    }
                }
                if (!hasBuilding)
                    continue;

                if (getBuildingLevel(upgradeBuildingId) <= hireTierMax) {
                    list.pushBack(hireList, &current->first);
                }
            }
        }
    }

    static std::optional<sol::environment> env;
    static std::optional<sol::function> processFunction;
    const std::filesystem::path path = scriptsFolder() / "hire.lua";

    if (!env && !processFunction) {
        processFunction = getScriptFunction(path, "getUnitsHireList", env, false, true);
    }

    if (processFunction) {
        try {
            auto fort = getFort(dataCache, fortificationId);
            lua_State* L = env->lua_state();
            sol::table inputList = idListToTable(L, hireList);
            bindings::FortView fortView(fort, dataCache);
            sol::table outputList = (*processFunction)(inputList, fortView);

            if (outputList.valid() && outputList.get_type() == sol::type::table) {
                tableToIdList(outputList, hireList);
            }
        } catch (const std::exception& e) {
            showErrorMessageBox(fmt::format("Failed to run script for units hire: {}", e.what()));
        }
    }

    return true;
}

bool __stdcall shouldAddUnitToHireHooked(const game::CMidPlayer* player,
                                         game::CPhaseGame* phaseGame,
                                         const game::CMidgardID* unitImplId)
{
    return true;
}

bool __stdcall enableUnitInHireListUiHooked(const game::CMidPlayer* player,
                                            game::CPhaseGame* phaseGame,
                                            const game::CMidgardID* unitImplId)
{
    using namespace game;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);

    auto playerBuildings = getPlayerBuildings(objectMap, player);
    if (!playerBuildings) {
        return false;
    }

    const auto& global = GlobalDataApi::get();
    const auto globalData = *global.getGlobalData();

    auto unitImpl = (const TUsUnitImpl*)global.findById(globalData->units, unitImplId);
    if (!unitImpl) {
        return false;
    }

    auto racialSoldier = gameFunctions().castUnitImplToRacialSoldier(unitImpl);
    if (!racialSoldier) {
        return false;
    }

    auto soldier = gameFunctions().castUnitImplToSoldier(unitImpl);
    if (!soldier) {
        return false;
    }

    auto enrollBuildingId = racialSoldier->vftable->getEnrollBuildingId(racialSoldier);

    // Starting units are always allowed
    if (soldier->vftable->getLevel(soldier) == 1 && *enrollBuildingId == emptyId) {
        return true;
    }

    // High tier units are always allowed
    if (soldier->vftable->getLevel(soldier) > 1) {
        return true;
    }

    // If unit has enroll bulding requirement, check it
    if (*enrollBuildingId != emptyId) {
        const auto& buildList = playerBuildings->buildings;
        for (auto node = buildList.head->next; node != buildList.head; node = node->next) {
            if (node->data == *enrollBuildingId) {
                return true;
            }
        }
        return false;
    }

    // High tier units have only upgrade building requirement:
    // upgrade building is required for units from previous tier to promote
    auto upgradeBuildingId = racialSoldier->vftable->getUpgradeBuildingId(racialSoldier);
    if (*upgradeBuildingId != emptyId) {
        const auto& buildList = playerBuildings->buildings;
        for (auto node = buildList.head->next; node != buildList.head; node = node->next) {
            if (node->data == *upgradeBuildingId) {
                return true;
            }
        }
    }

    return false;
}

void __stdcall addSideshowUnitToUIHooked(game::IdList* unitIds,
                                         game::CMidPlayer* player,
                                         game::CPhaseGame* phaseGame)
{
    return;
}

} // namespace hooks