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
#include "midgardid.h"
#include "midgardobjectmap.h"
#include "midplayer.h"
#include "midscenvariables.h"
#include "midvillage.h"
#include "originalfunctions.h"
#include "playerview.h"
#include "racecategory.h"
#include "racetype.h"
#include "scripts.h"
#include "settings.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <spdlog/spdlog.h>

extern std::thread::id mainThreadId;

namespace hooks {

game::Bank* __stdcall computePlayerDailyIncomeHooked(game::Bank* income,
                                                     game::IMidgardObjectMap* objectMap,
                                                     const game::CMidgardID* playerId)
{
    using namespace game;

    getOriginalFunctions().computePlayerDailyIncome(income, objectMap, playerId);

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
    CurrencyType manaType = CurrencyType::Gold;
    std::int32_t manaIncome = 0;

    if (races.neutral && raceId == races.neutral->id) {
        return income;
    } else if (races.human && raceId == races.human->id) {
        racePrefix = "EMPIRE_";
        manaType = CurrencyType::LifeMana;
        manaIncome = income->lifeMana;
    } else if (races.heretic && raceId == races.heretic->id) {
        racePrefix = "LEGIONS_";
        manaType = CurrencyType::InfernalMana;
        manaIncome = income->infernalMana;
    } else if (races.dwarf && raceId == races.dwarf->id) {
        racePrefix = "CLANS_";
        manaType = CurrencyType::RunicMana;
        manaIncome = income->runicMana;
    } else if (races.undead && raceId == races.undead->id) {
        racePrefix = "HORDES_";
        manaType = CurrencyType::DeathMana;
        manaIncome = income->deathMana;
    } else if (races.elf && raceId == races.elf->id) {
        racePrefix = "ELVES_";
        manaType = CurrencyType::GroveMana;
        manaIncome = income->groveMana;
    }

    if (!racePrefix) {
        spdlog::error("Trying to compute daily income for unknown race. LRace.dbf id: {:d}",
                      (int)raceId);
        return income;
    }

    int additionalGoldIncome{};
    int additionalManaIncome{};
    const auto& globalApi = GlobalDataApi::get();
    if (globalApi.getGlobalData && *globalApi.getGlobalData()) {
        const auto lords = (*globalApi.getGlobalData())->lords;
        if (lords) {
            const auto lordType = (TLordType*)globalApi.findById(lords, &player->lordId);
            if (lordType && lordType->data) {
                const auto lordId = lordType->data->lordCategory.id;
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
                default:
                    break;
                }
            }
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

        auto getVillageIncome = [playerId, cityGoldIncome, cityManaIncome, &additionalGoldIncome,
                                 &additionalManaIncome](const IMidScenarioObject* obj) {
            if (!obj) return;
            auto fortification = static_cast<const CFortification*>(obj);

            if (fortification->ownerId == *playerId) {
                auto vftable = static_cast<const CFortificationVftable*>(fortification->vftable);
                if (!vftable || !vftable->getCategory) return;
                auto category = vftable->getCategory(fortification);
                if (!category) return;

                if (FortCategories::get().village && category->id == FortCategories::get().village->id) {
                    auto village = static_cast<const CMidVillage*>(fortification);
                    if (village->tierLevel >= 0 && village->tierLevel < 6) {
                        additionalGoldIncome += cityGoldIncome[village->tierLevel];
                        additionalManaIncome += cityManaIncome[village->tierLevel];
                    }
                }
            }
        };

        forEachScenarioObject(objectMap, IdType::Fortification, getVillageIncome);

        additionalGoldIncome += cityGoldIncome[0];
        additionalManaIncome += cityManaIncome[0];
    }

    const int totalGoldIncome = income->gold + additionalGoldIncome;
    const int totalManaIncome = manaIncome + additionalManaIncome;

    BankApi::get().set(income, CurrencyType::Gold, std::clamp(totalGoldIncome, 0, 9999));
    BankApi::get().set(income, manaType, std::clamp(totalManaIncome, 0, 9999));

    static std::optional<sol::environment> env;
    static std::optional<sol::protected_function> getIncome;
    const auto path{scriptsFolder() / "income.lua"};
    if (!env && !getIncome) {
        env = executeScriptFile(path, false, true);
        if (env) {
            getIncome = getProtectedScriptFunction(*env, "getTurnIncome", false);
        }
    }
    if (getIncome && getIncome->valid()) {
        bindings::PlayerView playerView{player, objectMap};
        bindings::CurrencyView incomeView{*income};
        try {
            bool isInterfaceCall = (std::this_thread::get_id() == mainThreadId);
            sol::protected_function_result result = (*getIncome)(playerView, incomeView, isInterfaceCall);
            if (result.valid()) {
                sol::object obj = result;
                if (obj.is<bindings::CurrencyView>()) {
                    const bindings::CurrencyView& resultView = obj.as<bindings::CurrencyView>();
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
            } else {
                sol::error err = result;
                spdlog::error("[INCOME] Lua error: {}", err.what());
            }
        } catch (const std::exception& e) {
            showErrorMessageBox(fmt::format("Failed to run '{:s}' script.\n"
                                            "Reason: '{:s}'",
                                            path.string(), e.what()));
        } catch (...) {
            spdlog::error("[INCOME] Lua unknown exception");
        }
    }

    return income;
}

} // namespace hooks
