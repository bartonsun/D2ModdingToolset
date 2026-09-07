/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Vladimir Makeev.
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

#include "playerincomehooks.h"
#include "currency.h"
#include "currencyview.h"
#include "fortcategory.h"
#include "fortification.h"
#include "game.h"
#include "gameutils.h"
#include "lordtype.h"
#include "midgard.h"
#include "midgardid.h"
#include "midgardobjectmap.h"
#include "midplayer.h"
#include "midscenvariables.h"
#include "midserverlogic.h"
#include "midvillage.h"
#include "netplayerinfo.h"
#include "originalfunctions.h"
#include "playerview.h"
#include "racecategory.h"
#include "racetype.h"
#include "scenarioinfo.h"
#include "scripts.h"
#include "settings.h"
#include "utils.h"
#include "version.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <intrin.h>
#include <map>
#include <mutex>
#include <spdlog/spdlog.h>

extern std::thread::id mainThreadId;

namespace hooks {

namespace {

const game::IMidgardObjectMap* incomeScenario{};
std::map<int, int> creditedTurns;
std::mutex incomeMutex;
thread_local game::CMidServerLogic* restoringServer{};
thread_local const game::IMidgardObjectMap* restoredScenario{};
thread_local int restoredTurn{-1};

bool isNetworkGame()
{
    const auto* midgard = game::CMidgardApi::get().instance();
    return midgard && midgard->data && midgard->data->multiplayerGame
           && !midgard->data->hotseatGame;
}

void selectIncomeScenario(const game::IMidgardObjectMap* objectMap)
{
    if (incomeScenario != objectMap) {
        incomeScenario = objectMap;
        creditedTurns.clear();
    }
}

void markRestoredCurrentPlayer()
{
    const auto* server = restoringServer;
    if (!server || !server->coreData) {
        return;
    }

    const auto* core = server->coreData;
    const auto* players = core->players;
    const int index = server->currentPlayerIndex;
    if (!core->multiplayerGame || core->hotseatGame || !core->objectMap
        || core->objectMap != restoredScenario || !players || !players->bgn || !players->end
        || index < 0 || static_cast<std::size_t>(index) >= players->size()) {
        return;
    }

    const auto* info = getScenarioInfo(core->objectMap);
    if (!info || info->currentTurn <= 0 || info->currentTurn != restoredTurn) {
        return;
    }

    const auto& playerId = players->bgn[index].playerId;
    const std::lock_guard<std::mutex> lock{incomeMutex};
    selectIncomeScenario(core->objectMap);
    const auto it = creditedTurns.find(playerId.value);
    if (it != creditedTurns.end() && it->second == restoredTurn) {
        return;
    }
    creditedTurns[playerId.value] = info->currentTurn;
    spdlog::info("Preserve restored daily income for {} on turn {}",
                 idToString(&playerId), restoredTurn);
}

bool claimDailyIncome(const game::IMidgardObjectMap* objectMap,
                      const game::CMidgardID* playerId)
{
    const auto* info = getScenarioInfo(objectMap);
    if (!info) {
        return true;
    }

    markRestoredCurrentPlayer();
    const std::lock_guard<std::mutex> lock{incomeMutex};
    selectIncomeScenario(objectMap);
    const auto it = creditedTurns.find(playerId->value);
    if (it != creditedTurns.end() && it->second == info->currentTurn) {
        spdlog::info("Skip repeated daily income for {} on turn {}",
                     idToString(playerId), info->currentTurn);
        return false;
    }

    creditedTurns[playerId->value] = info->currentTurn;
    return true;
}

bool isDailyIncomeCreditCall(std::uintptr_t returnAddress)
{
    switch (gameVersion()) {
    case GameVersion::Akella:
    case GameVersion::Russobit:
        return returnAddress == 0x43b0e1;
    case GameVersion::Gog:
        return returnAddress == 0x43aba7;
    default:
        return false;
    }
}

} // namespace

void resetDailyIncomeTracking()
{
    const std::lock_guard<std::mutex> lock{incomeMutex};
    incomeScenario = nullptr;
    creditedTurns.clear();
}

RestoredGameIncomeScope::RestoredGameIncomeScope(game::CMidServerLogic* serverLogic)
    : previous{restoringServer}
    , previousScenario{restoredScenario}
    , previousTurn{restoredTurn}
{
    restoringServer = serverLogic;
    restoredScenario = serverLogic && serverLogic->coreData
                           ? serverLogic->coreData->objectMap : nullptr;
    const auto* info = restoredScenario ? getScenarioInfo(restoredScenario) : nullptr;
    restoredTurn = info ? info->currentTurn : -1;
}

RestoredGameIncomeScope::~RestoredGameIncomeScope()
{
    // The native loader sets currentPlayerIndex before sending BeginTurn. Record the
    // saved player's income before a queued acknowledgement can start their turn again.
    markRestoredCurrentPlayer();
    restoringServer = previous;
    restoredScenario = previousScenario;
    restoredTurn = previousTurn;
}

bool wasPlayerIncomeCredited(const game::IMidgardObjectMap* objectMap,
                            const game::CMidgardID* playerId)
{
    if (!objectMap || !playerId || !isNetworkGame()) {
        return false;
    }

    const auto* info = getScenarioInfo(objectMap);
    if (!info) {
        return false;
    }

    markRestoredCurrentPlayer();
    const std::lock_guard<std::mutex> lock{incomeMutex};
    if (incomeScenario != objectMap) {
        return false;
    }

    const auto it = creditedTurns.find(playerId->value);
    return it != creditedTurns.end() && it->second == info->currentTurn;
}

game::Bank* __stdcall computePlayerDailyIncomeHooked(game::Bank* income,
                                                     game::IMidgardObjectMap* objectMap,
                                                     const game::CMidgardID* playerId)
{
    using namespace game;

    if (isDailyIncomeCreditCall(reinterpret_cast<std::uintptr_t>(_ReturnAddress()))
        && isNetworkGame() && objectMap && playerId && !claimDailyIncome(objectMap, playerId)) {
        BankApi::get().setZero(income);
        return income;
    }

    getOriginalFunctions().computePlayerDailyIncome(income, objectMap, playerId);

    auto playerObj = objectMap->vftable->findScenarioObjectById(objectMap, playerId);
    if (!playerObj) {
        spdlog::error("Could not find player {:s}", idToString(playerId));
        return income;
    }

    const auto& races = RaceCategories::get();
    auto player = static_cast<const CMidPlayer*>(playerObj);
    const auto raceId = player->raceType->data->raceType.id;
    const char* racePrefix{};
    const char* lordPrefix{};
    CurrencyType manaType;
    std::int32_t manaIncome;

    if (raceId == races.neutral->id) {
        // Skip neutrals
        return income;
    } else if (raceId == races.human->id) {
        racePrefix = "EMPIRE_";
        manaType = CurrencyType::LifeMana;
        manaIncome = income->lifeMana;
    } else if (raceId == races.heretic->id) {
        racePrefix = "LEGIONS_";
        manaType = CurrencyType::InfernalMana;
        manaIncome = income->infernalMana;
    } else if (raceId == races.dwarf->id) {
        racePrefix = "CLANS_";
        manaType = CurrencyType::RunicMana;
        manaIncome = income->runicMana;
    } else if (raceId == races.undead->id) {
        racePrefix = "HORDES_";
        manaType = CurrencyType::DeathMana;
        manaIncome = income->deathMana;
    } else if (raceId == races.elf->id) {
        racePrefix = "ELVES_";
        manaType = CurrencyType::GroveMana;
        manaIncome = income->groveMana;
    }

    if (!racePrefix) {
        spdlog::error("Trying to compute daily income for unknown race. LRace.dbf id: {:d}",
                      (int)raceId);
        return income;
    }

    // additional income for lord type
    const auto& globalApi = GlobalDataApi::get();
    const auto lords = (*globalApi.getGlobalData())->lords;
    const auto lordType = (TLordType*)globalApi.findById(lords, &player->lordId);
    const auto lordId = lordType->data->lordCategory.id;
    int additionalGoldIncome{};
    int additionalManaIncome{};
    switch (lordId) {
    case LordId::Warrior:
        lordPrefix = "WARRIOR";
        additionalGoldIncome = gameSettings().additionalLordIncome.gold.warrior;
        additionalManaIncome = gameSettings().additionalLordIncome.mana.warrior;
        break;
    case LordId::Mage:
        lordPrefix = "MAGE";
        additionalGoldIncome = gameSettings().additionalLordIncome.gold.mage;
        additionalManaIncome = gameSettings().additionalLordIncome.mana.mage;
        break;
    case LordId::Diplomat:
        lordPrefix = "GUILDMASTER";
        additionalGoldIncome = gameSettings().additionalLordIncome.gold.guildmaster;
        additionalManaIncome = gameSettings().additionalLordIncome.mana.guildmaster;
        break;
    }

    std::array<int, 6> cityGoldIncome = {gameSettings().additionalCityIncome.gold.capital,
                                         gameSettings().additionalCityIncome.gold.tier1,
                                         gameSettings().additionalCityIncome.gold.tier2,
                                         gameSettings().additionalCityIncome.gold.tier3,
                                         gameSettings().additionalCityIncome.gold.tier4,
                                         gameSettings().additionalCityIncome.gold.tier5};

    std::array<int, 6> cityManaIncome = {gameSettings().additionalCityIncome.mana.capital,
                                         gameSettings().additionalCityIncome.mana.tier1,
                                         gameSettings().additionalCityIncome.mana.tier2,
                                         gameSettings().additionalCityIncome.mana.tier3,
                                         gameSettings().additionalCityIncome.mana.tier4,
                                         gameSettings().additionalCityIncome.mana.tier5};

    auto variables{getScenarioVariables(objectMap)};

    std::uint32_t listIndex{};
    if (variables && variables->variables.length) {
        spdlog::debug("Loop through {:d} scenario variables", variables->variables.length);
        for (const auto& variable : variables->variables) {
            const auto& name = variable.second.name;

            // Additional income for specific lord
            if (!strncmp(name, lordPrefix, std::strlen(lordPrefix))) {
                const auto expectedNameGold{fmt::format("{:s}_GOLD_INCOME", lordPrefix)};
                const auto expectedNameMana{fmt::format("{:s}_MANA_INCOME", lordPrefix)};
                if (!strncmp(name, expectedNameGold.c_str(), sizeof(name))) {
                    additionalGoldIncome += variable.second.value;
                    continue;
                } else if (!strncmp(name, expectedNameMana.c_str(), sizeof(name))) {
                    additionalManaIncome += variable.second.value;
                    continue;
                }
            }

            // Additional city income for specific race
            if (!strncmp(name, racePrefix, std::strlen(racePrefix))) {
                for (int i = 0; i < 6; ++i) {
                    const auto expectedName{
                        fmt::format("{:s}TIER_{:d}_CITY_INCOME", racePrefix, i)};
                    const auto expectedNameGold{
                        fmt::format("{:s}TIER_{:d}_CITY_GOLD_INCOME", racePrefix, i)};
                    const auto expectedNameMana{
                        fmt::format("{:s}TIER_{:d}_CITY_MANA_INCOME", racePrefix, i)};
                    if (!strncmp(name, expectedName.c_str(), sizeof(name))) {
                        cityGoldIncome[i] += variable.second.value;
                        break;
                    } else if (!strncmp(name, expectedNameGold.c_str(), sizeof(name))) {
                        cityGoldIncome[i] += variable.second.value;
                        break;
                    } else if (!strncmp(name, expectedNameMana.c_str(), sizeof(name))) {
                        cityManaIncome[i] += variable.second.value;
                        break;
                    }
                }
            }

            // Additional city income for all races
            if (!strncmp(name, "TIER", 4)) {
                for (int i = 0; i < 6; ++i) {
                    const auto expectedName{fmt::format("TIER_{:d}_CITY_INCOME", i)};
                    const auto expectedNameGold{fmt::format("TIER_{:d}_CITY_GOLD_INCOME", i)};
                    const auto expectedNameMana{fmt::format("TIER_{:d}_CITY_MANA_INCOME", i)};

                    if (!strncmp(name, expectedName.c_str(), sizeof(name))) {
                        cityGoldIncome[i] += variable.second.value;
                        break;
                    } else if (!strncmp(name, expectedNameGold.c_str(), sizeof(name))) {
                        cityGoldIncome[i] += variable.second.value;
                        break;
                    } else if (!strncmp(name, expectedNameMana.c_str(), sizeof(name))) {
                        cityManaIncome[i] += variable.second.value;
                        break;
                    }
                }
            }

            listIndex++;
        }
        spdlog::debug("Loop done in {:d} iterations", listIndex);

        // Custom cities income
        auto getVillageIncome = [playerId, cityGoldIncome, cityManaIncome, &additionalGoldIncome,
                                 &additionalManaIncome](const IMidScenarioObject* obj) {
            auto fortification = static_cast<const CFortification*>(obj);

            if (fortification->ownerId == *playerId) {
                auto vftable = static_cast<const CFortificationVftable*>(fortification->vftable);
                auto category = vftable->getCategory(fortification);

                if (category->id == FortCategories::get().village->id) {
                    auto village = static_cast<const CMidVillage*>(fortification);

                    additionalGoldIncome += cityGoldIncome[village->tierLevel];
                    additionalManaIncome += cityManaIncome[village->tierLevel];
                }
            }
        };

        forEachScenarioObject(objectMap, IdType::Fortification, getVillageIncome);

        // Custom capital city income
        additionalGoldIncome += cityGoldIncome[0];
        additionalManaIncome += cityManaIncome[0];
    }

    const int totalGoldIncome = income->gold + additionalGoldIncome;
    const int totalManaIncome = manaIncome + additionalManaIncome;

    BankApi::get().set(income, CurrencyType::Gold, std::clamp(totalGoldIncome, 0, 9999));
    BankApi::get().set(income, manaType, std::clamp(totalManaIncome, 0, 9999));

    static std::optional<sol::environment> env;
    static std::optional<sol::function> getIncome;
    const auto path{scriptsFolder() / "income.lua"};
    if (!env && !getIncome) {
        getIncome = getScriptFunction(path, "getTurnIncome", env, false, true);
    }
    if (getIncome) {
        bindings::PlayerView playerView{player, objectMap};
        bindings::CurrencyView incomeView{*income};
        try {
            bool isInterfaceCall = (std::this_thread::get_id() == mainThreadId);
            sol::object result = (*getIncome)(playerView, incomeView, isInterfaceCall);
            if (result.is<bindings::CurrencyView>()) {
                const bindings::CurrencyView& resultView = result.as<bindings::CurrencyView>();
                income->gold = resultView.bank.gold;
                income->infernalMana = resultView.bank.infernalMana;
                income->lifeMana = resultView.bank.lifeMana;
                income->deathMana = resultView.bank.deathMana;
                income->runicMana = resultView.bank.runicMana;
                income->groveMana = resultView.bank.groveMana;
                spdlog::debug("Income modified by Lua");
            } else {
                spdlog::debug("Lua function did not return a Currency object, income unchanged");
            }
        } catch (const std::exception& e) {
            showErrorMessageBox(fmt::format("Failed to run '{:s}' script.\n"
                                            "Reason: '{:s}'",
                                            path.string(), e.what()));
        }
    }

    return income;
}

} // namespace hooks
