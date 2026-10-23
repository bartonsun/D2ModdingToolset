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
#include "hooks.h"
#include "lordtype.h"
#include "midgard.h"
#include "midgardid.h"
#include "midgardobjectmap.h"
#include "midplayer.h"
#include "midscenvariables.h"
#include "midserver.h"
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
#include "streambits.h"
#include "utils.h"
#include "version.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <intrin.h>
#include <map>
#include <mutex>
#include <set>
#include <spdlog/spdlog.h>

extern std::thread::id mainThreadId;

namespace hooks {

namespace {

// Network game state, cleared when the game is left.  Player ids are keys.
std::mutex incomeMutex;
std::map<int, int> incomeTurns;  // turn of the last daily income
std::map<int, int> startedTurns; // turn of the last turn.lua run
std::set<int> humanPlayers;
enum class StartingResources { Unknown, Pending, Granted } startingResources;

thread_local const game::CMidServerLogic* turnServer{};
thread_local game::CMidServerLogic* restoringServer{};
thread_local int restoredTurn{-1};
thread_local bool restoredPlayerMarked{};

using GrantStartingResources = void(__stdcall*)(game::IMidgardObjectMap*, int gold, int mana);
GrantStartingResources grantStartingResourcesOriginal{};

using ComputerTurnBonus = void(__stdcall*)(game::IMidgardObjectMap*,
                                           void* changes,
                                           const game::CMidgardID* playerId);
ComputerTurnBonus computerTurnBonusOriginal{};

using ReadNetMessage = bool(__stdcall*)(game::CNetMsg*, game::CMqStream*);
ReadNetMessage readNetMessageOriginal{};

bool isRetail()
{
    return gameVersion() == GameVersion::Akella || gameVersion() == GameVersion::Russobit;
}

bool isNetworkGame(const game::CMidServerLogic* logic)
{
    return logic && logic->coreData && logic->coreData->multiplayerGame
           && !logic->coreData->hotseatGame;
}

const game::CMidServerLogic* incomeServer()
{
    if (turnServer) {
        return turnServer;
    }
    if (restoringServer) {
        return restoringServer;
    }
    const auto* midgard = game::CMidgardApi::get().instance();
    if (!midgard || !midgard->data || !midgard->data->server || !midgard->data->server->data) {
        return nullptr;
    }
    return midgard->data->server->data->serverLogic;
}

int currentTurn(const game::CMidServerLogic* logic)
{
    const auto* info = logic->coreData->objectMap ? getScenarioInfo(logic->coreData->objectMap)
                                                  : nullptr;
    return info ? info->currentTurn : -1;
}

bool wasClaimed(const std::map<int, int>& turns, int turn, int playerId)
{
    const auto found = turns.find(playerId);
    return found != turns.end() && found->second == turn;
}

bool claim(std::map<int, int>& turns, int turn, int playerId)
{
    if (wasClaimed(turns, turn, playerId)) {
        return false;
    }
    turns[playerId] = turn;
    return true;
}

// A restored game continues the turn of its current player, who got the
// day's income before the save or the disconnect.
void markRestoredPlayer()
{
    const auto* logic = restoringServer;
    if (restoredPlayerMarked || !logic || currentTurn(logic) != restoredTurn) {
        return;
    }
    const auto* players = logic->coreData->players;
    const int index = logic->currentPlayerIndex;
    if (!players || !players->bgn || index < 0
        || static_cast<std::size_t>(index) >= players->size()) {
        return;
    }

    restoredPlayerMarked = true;
    const auto playerId = players->bgn[index].playerId.value;
    const std::lock_guard<std::mutex> lock{incomeMutex};
    claim(incomeTurns, restoredTurn, playerId);
    claim(startedTurns, restoredTurn, playerId);
}

bool isDailyIncomeCredit(std::uintptr_t returnAddress)
{
    if (isRetail()) {
        return returnAddress == 0x43b0e1;
    }
    return gameVersion() == GameVersion::Gog && returnAddress == 0x43aba7;
}

// Zero when the income of this day was already credited.  A restored game
// replays its turn start without income.
bool suppressDailyIncome(const game::IMidgardObjectMap* objectMap,
                         const game::CMidgardID* playerId,
                         std::uintptr_t returnAddress)
{
    const auto* logic = incomeServer();
    if (!objectMap || !playerId || !isNetworkGame(logic)
        || (!restoringServer && !isDailyIncomeCredit(returnAddress))) {
        return false;
    }
    const auto* info = getScenarioInfo(objectMap);
    if (!info) {
        return false;
    }

    markRestoredPlayer();
    const std::lock_guard<std::mutex> lock{incomeMutex};
    return !claim(incomeTurns, info->currentTurn, playerId->value) || restoringServer;
}

void __stdcall grantStartingResourcesHooked(game::IMidgardObjectMap* objectMap, int gold, int mana)
{
    // Day zero may advance the day before this grant, so the day is observed
    // when the day-zero processing starts (observeStartingResources).
    bool grant = true;
    if (isNetworkGame(incomeServer())) {
        const auto* info = objectMap ? getScenarioInfo(objectMap) : nullptr;
        const int turn = info ? info->currentTurn : -1;
        const std::lock_guard<std::mutex> lock{incomeMutex};
        grant = startingResources == StartingResources::Pending
                || (startingResources == StartingResources::Unknown && turn <= 0);
        startingResources = StartingResources::Granted;
    }
    if (grant) {
        grantStartingResourcesOriginal(objectMap, gold, mana);
    }
}

bool isHumanPlayer(const game::CMidServerLogic* logic, const game::CMidgardID* playerId)
{
    rememberHumanPlayers(logic);
    const std::lock_guard<std::mutex> lock{incomeMutex};
    return humanPlayers.count(playerId->value) != 0;
}

// The host gives a dropped player to the computer until he reconnects, and
// the computer's turn start adds its difficulty bonus to the bank.
void __stdcall computerTurnBonusHooked(game::IMidgardObjectMap* objectMap,
                                       void* changes,
                                       const game::CMidgardID* playerId)
{
    const auto* logic = incomeServer();
    if (playerId && isNetworkGame(logic) && isHumanPlayer(logic, playerId)) {
        return;
    }
    computerTurnBonusOriginal(objectMap, changes, playerId);
}

bool isReplayRead(std::uintptr_t returnAddress)
{
    if (isRetail()) {
        return returnAddress == 0x421f5e || returnAddress == 0x421fbb;
    }
    return gameVersion() == GameVersion::Gog
           && (returnAddress == 0x421a7c || returnAddress == 0x421ad9);
}

// The reconnect replay reads the server's cached command streams, whose read
// position stays at the end after the first reconnect.  Read a copy.
bool __stdcall replayReadHooked(game::CNetMsg* message, game::CMqStream* stream)
{
    if (!restoringServer || !message || !stream || !stream->read
        || !isReplayRead(reinterpret_cast<std::uintptr_t>(_ReturnAddress()))) {
        return readNetMessageOriginal(message, stream);
    }

    const auto& api = game::CStreamBitsApi::get();
    game::CStreamBits reader{};
    api.readConstructor(&reader, 0, stream->vftable->getBuffer(stream),
                        stream->vftable->getNumBytes(stream), false);
    struct Guard
    {
        game::CStreamBits* reader;
        ~Guard()
        {
            game::CStreamBitsApi::get().destructor(reader);
        }
    } guard{&reader};
    return readNetMessageOriginal(message, &reader);
}

} // namespace

void addNetworkIncomeHooks(std::vector<HookInfo>& hooks)
{
    const bool retail = isRetail();
    if (!retail && gameVersion() != GameVersion::Gog) {
        return;
    }

    hooks.push_back({reinterpret_cast<void*>(retail ? 0x42bd32 : 0x42b74f),
                     reinterpret_cast<void*>(grantStartingResourcesHooked),
                     reinterpret_cast<void**>(&grantStartingResourcesOriginal)});
    hooks.push_back({reinterpret_cast<void*>(retail ? 0x55ce2a : 0x55c61e),
                     reinterpret_cast<void*>(replayReadHooked),
                     reinterpret_cast<void**>(&readNetMessageOriginal)});

    static constexpr unsigned char bonusCode[] = {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56,
                                                  0x8b, 0x75, 0x08, 0x57, 0x56, 0xe8};
    auto* bonus = reinterpret_cast<void*>(0x473b34);
    if (retail && !std::memcmp(bonus, bonusCode, sizeof(bonusCode))) {
        hooks.push_back({bonus, reinterpret_cast<void*>(computerTurnBonusHooked),
                         reinterpret_cast<void**>(&computerTurnBonusOriginal)});
    }
}

void resetNetworkIncome()
{
    const std::lock_guard<std::mutex> lock{incomeMutex};
    incomeTurns.clear();
    startedTurns.clear();
    humanPlayers.clear();
    startingResources = StartingResources::Unknown;
}

// Every player is connected when a turn starts, so this is where the human
// players are learned before anyone can drop.
void rememberHumanPlayers(const game::CMidServerLogic* logic)
{
    if (!isNetworkGame(logic) || !logic->coreData->players) {
        return;
    }
    const auto* players = logic->coreData->players;
    const std::lock_guard<std::mutex> lock{incomeMutex};
    for (const auto* info = players->bgn; info && info != players->end; ++info) {
        if (info->controlledByHuman) {
            humanPlayers.insert(info->playerId.value);
        }
    }
}

void observeStartingResources(const game::CMidServerLogic* logic, int turn)
{
    if (!isNetworkGame(logic)) {
        return;
    }
    const std::lock_guard<std::mutex> lock{incomeMutex};
    if (startingResources == StartingResources::Unknown) {
        startingResources = turn == 0 ? StartingResources::Pending : StartingResources::Granted;
    }
}

bool isIncomeCredited(const game::CMidServerLogic* logic, const game::CMidgardID* playerId)
{
    if (!playerId || !isNetworkGame(logic)) {
        return false;
    }
    markRestoredPlayer();
    const std::lock_guard<std::mutex> lock{incomeMutex};
    return wasClaimed(incomeTurns, currentTurn(logic), playerId->value);
}

bool claimTurnStart(const game::CMidServerLogic* logic, const game::CMidgardID* playerId)
{
    if (!playerId || !isNetworkGame(logic)) {
        return true;
    }
    markRestoredPlayer();
    const std::lock_guard<std::mutex> lock{incomeMutex};
    return claim(startedTurns, currentTurn(logic), playerId->value);
}

TurnIncomeScope::TurnIncomeScope(const game::CMidServerLogic* logic)
    : previous{turnServer}
{
    turnServer = logic;
}

TurnIncomeScope::~TurnIncomeScope()
{
    turnServer = previous;
}

RestoredGameScope::RestoredGameScope(game::CMidServerLogic* logic, bool currentPlayerReady)
    : previous{restoringServer}
    , previousTurn{restoredTurn}
    , previousMarked{restoredPlayerMarked}
{
    restoringServer = logic;
    restoredTurn = logic ? currentTurn(logic) : -1;
    restoredPlayerMarked = false;
    if (currentPlayerReady) {
        markRestoredPlayer();
    }
}

RestoredGameScope::~RestoredGameScope()
{
    markRestoredPlayer();
    restoringServer = previous;
    restoredTurn = previousTurn;
    restoredPlayerMarked = previousMarked;
}

game::Bank* __stdcall computePlayerDailyIncomeHooked(game::Bank* income,
                                                     game::IMidgardObjectMap* objectMap,
                                                     const game::CMidgardID* playerId)
{
    using namespace game;

    if (suppressDailyIncome(objectMap, playerId,
                            reinterpret_cast<std::uintptr_t>(_ReturnAddress()))) {
        BankApi::get().setZero(income);
        return income;
    }

    getOriginalFunctions().computePlayerDailyIncome(income, objectMap, playerId);

    // The day-2 crash from T-32 walked in through here: the hook runs on turn
    // boundaries, when the object map can be mid-swap, and a vcall on a null
    // map is indistinguishable from the game's own fault in the dump.
    if (!objectMap || !objectMap->vftable || !objectMap->vftable->findScenarioObjectById || !playerId) {
        return income;
    }

    auto playerObj = objectMap->vftable->findScenarioObjectById(objectMap, playerId);
    if (!playerObj) {
        spdlog::error("Could not find player {:s}", idToString(playerId));
        return income;
    }

    const auto& races = RaceCategories::get();
    auto player = static_cast<const CMidPlayer*>(playerObj);
    if (!player || !player->raceType || !player->raceType->data) {
        return income;
    }
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
    int additionalGoldIncome{};
    int additionalManaIncome{};
    // findById returns null for a lord the scenario does not carry; the switch
    // below then read through it.
    if (lordType && lordType->data) {
        switch (lordType->data->lordCategory.id) {
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
            if (lordPrefix && !strncmp(name, lordPrefix, std::strlen(lordPrefix))) {
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
            if (racePrefix && !strncmp(name, racePrefix, std::strlen(racePrefix))) {
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

    // A protected_function, not sol::function: under NDEBUG the latter is
    // sol::unsafe_function, and an error inside it leaves the Lua stack
    // unwound past the try/catch -- the Release-only crash T-32 traced.
    static std::optional<sol::environment> env;
    static std::optional<sol::protected_function> getIncome;
    const auto path{scriptsFolder() / "income.lua"};
    if (!env && !getIncome) {
        env = executeScriptFile(path, false, true);
        if (env) {
            getIncome = getProtectedScriptFunction(env.value(), "getTurnIncome", false);
        }
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
