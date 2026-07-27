/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/bartonsun/D2ModdingToolset)
 * Copyright (C) 2026 Max Vynogradov.
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

// ============================================================
// File: stealitemhooks.cpp
// ============================================================

#include "stealitemhooks.h"

#include "stealmerchantiteminterf.h"

#include "button.h"
#include "dialoginterf.h"

#include "midpopupinterf.h"
#include "draganddropinterf.h"
#include "game.h"
#include "gameutils.h"
#include "globaldata.h"
#include "globalvariables.h"
#include "itembase.h"
#include "itembaseview.h"
#include "midgardobjectmap.h"
#include "midplayer.h"
#include "midsite.h"
#include "midsitemerchant.h"
#include "phasegame.h"
#include "playerview.h"
#include "scripts.h"
#include "task.h"
#include "utils.h"

#include <algorithm>
#include <merchantview.h>
#include <optional>
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace hooks {

thread_local StealItemBuildContext g_stealItemCtx;

//
// lua
//

static std::optional<sol::environment> env;

static std::optional<sol::function> theftFilterItemsMerchantLua;

static int getItemTotalCost(const game::StealItemEntry* item)
{
    if (!item) {
        return 0;
    }

    const auto& cost = item->value;

    return cost.gold + cost.infernalMana + cost.lifeMana + cost.runicMana + cost.deathMana
           + cost.groveMana;
}

//
// scenario vars
//

static const char scenItemCanStealLessCostSum[]{"ITEM_CAN_STEAL_LESS_COST_SUM"};

static constexpr int noItemCostLimit{9999 * 6};

//
// ctor hook
//

using StealItemCtor = game::StealMerchantItemInterfApi::Api::Constructor;

static StealItemCtor stealItemCtorOrig;

//
// add item hook
//

using AddStealItemFunc = game::StealMerchantItemInterfApi::Api::AddStealItem;

static AddStealItemFunc addStealItemOrig;

//
// helpers
//

static bool getScenarioVariable(const game::IMidgardObjectMap* objectMap,
                                const char* name,
                                int& value)
{
    const auto scenVars{getScenarioVariables(objectMap)};

    if (!scenVars) {
        return false;
    }

    for (const auto& pair : scenVars->variables) {

        if (_stricmp(pair.second.name, name) == 0) {

            value = pair.second.value;

            return true;
        }
    }

    return false;
}

static int getItemCostLimit(const game::IMidgardObjectMap* objectMap)
{
    int value{};

    if (getScenarioVariable(objectMap, scenItemCanStealLessCostSum, value)) {
        return std::max(0, value);
    }

    const game::GlobalData* global{*game::GlobalDataApi::get().getGlobalData()};

    return global->globalVariables->data->stealItemValue;
}

//
// filtering
//

bool shouldDisplayStealItem(game::StealItemEntry* entry)
{
    if (!entry) {

        spdlog::info("[STEAL_ITEM] entry null");

        return false;
    }

    //
    // lua load
    //

    if (!theftFilterItemsMerchantLua) {

        const auto path{scriptsFolder() / "theft.lua"};

        theftFilterItemsMerchantLua = getScriptFunction(path, "theftFilterItemsMerchant", env,
                                                        false, true);

        if (theftFilterItemsMerchantLua) {

            spdlog::info("[STEAL_ITEM] lua loaded");

        } else {

            spdlog::info("[STEAL_ITEM] lua missing");
        }
    }

    if (!g_stealItemCtx.objectMap) {
        return false;
    }

    auto objectMap = g_stealItemCtx.objectMap;

    auto playerObj = objectMap->vftable->findScenarioObjectById(objectMap,
                                                                &g_stealItemCtx.playerId);

    auto merchantObj = objectMap->vftable->findScenarioObjectById(objectMap,
                                                                  &g_stealItemCtx.merchantId);

    auto player = static_cast<game::CMidPlayer*>(playerObj);

    auto merchant = static_cast<game::CMidSite*>(merchantObj);

    const auto global = *game::GlobalDataApi::get().getGlobalData();
    auto item = game::GlobalDataApi::get().findItemById(global->itemTypes, &entry->itemId);

    if (!playerObj || !merchantObj || !item) {
        return false;
    }

    //
    // Lua context
    //
    // Exposed objects:
    //
    // player   - PlayerView
    // merchant - MerchantView
    // item     - ItemBaseView
    //
    if (theftFilterItemsMerchantLua) {

        try {

            //
            sol::state_view lua(theftFilterItemsMerchantLua->lua_state());

            sol::table stealItemContext = lua.create_table();

            stealItemContext["player"] = bindings::PlayerView(player, objectMap);

            stealItemContext["merchant"] = bindings::MerchantView(
                static_cast<const game::CMidSiteMerchant*>(merchant), objectMap);

            stealItemContext["item"] = bindings::ItemBaseView(item);

            sol::object result = (*theftFilterItemsMerchantLua)(stealItemContext);

            if (result.is<bool>()) {
                return result.as<bool>();
            }

        } catch (const std::exception& e) {

            spdlog::info("[STEAL_ITEM] lua exception");
            spdlog::info(e.what());

            const auto path{scriptsFolder() / "theft.lua"};

            showErrorMessageBox(fmt::format("Failed to run "
                                            "'{:s}' script.\n"
                                            "Reason: '{:s}'",
                                            path.string(), e.what()));
        }
    }

    //
    // cpp fallback
    //

    const int limit = getItemCostLimit(g_stealItemCtx.objectMap);

    //
    // disabled
    //

    if (limit >= noItemCostLimit) {

        return true;
    }

    //
    // value check
    //

    int totalCost = getItemTotalCost(entry);

    return totalCost < limit;
}

//
// add item hook
//

char* __fastcall addStealItemHooked(void* thisptr, void*, game::StealItemEntry* entry)
{
    //
    // not our build
    //

    if (!g_stealItemCtx.active) {

        return addStealItemOrig(thisptr, entry);
    }

    //
    // filter
    //

    if (!shouldDisplayStealItem(entry)) {
        return reinterpret_cast<char*>(thisptr);
    }

    //
    // visible
    //

    ++g_stealItemCtx.insertedCount;

    //
    // original
    //

    return addStealItemOrig(thisptr, entry);
}

//
// ctor hook
//

game::CMidDragDropInterf* __fastcall stealItemCtorHooked(void* thisptr,
                                                         void*,
                                                         int a2,
                                                         int functor,
                                                         int a4,
                                                         const game::CMidgardID* merchantId)
{
    //
    // activate
    //

    g_stealItemCtx.active = true;
    g_stealItemCtx.insertedCount = 0;

    //
    // context
    //

    g_stealItemCtx.objectMap = getObjectMap();

    if (merchantId) {
        g_stealItemCtx.merchantId = *merchantId;
    }

    auto phaseGame = reinterpret_cast<game::CPhaseGame*>(a4);

    if (phaseGame) {

        if (auto playerId = game::CPhaseApi::get().getCurrentPlayerId(&phaseGame->phase)) {
            g_stealItemCtx.playerId = *playerId;
        }
    }

    //
    // original ctor
    //

    auto result = stealItemCtorOrig(thisptr, a2, functor, a4, merchantId);

    //
    // final state
    //

    g_stealItemCtx.lastBuildHadVisibleItems = g_stealItemCtx.insertedCount > 0;

    //
    // disable button
    //

    if (!g_stealItemCtx.lastBuildHadVisibleItems) {

        auto popup = reinterpret_cast<game::CMidPopupInterf*>(result);

        if (popup && popup->popupData) {

            auto dialog = popup->popupData->dialog;

            if (dialog) {

                const auto& dlgApi = game::CDialogInterfApi::get();

                if (auto okBtn = dlgApi.findButton(dialog, "BTN_YES")) {
                    okBtn->vftable->setEnabled(okBtn, false);
                }
            }
        }
    }

    //
    // cleanup
    //

    g_stealItemCtx.active = false;

    return result;
}

//
// exports
//

void* getStealItemCtorHooked()
{
    return (void*)stealItemCtorHooked;
}

void** getStealItemCtorOrig()
{
    return (void**)&stealItemCtorOrig;
}

void* getAddStealItemHooked()
{
    return (void*)addStealItemHooked;
}

void** getAddStealItemOrig()
{
    return (void**)&addStealItemOrig;
}

} // namespace hooks