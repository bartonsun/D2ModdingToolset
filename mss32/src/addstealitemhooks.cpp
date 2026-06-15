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

#include "addstealitemhooks.h"

#include "addstealitem.h"
#include "game.h"
#include "gameutils.h"
#include "globaldata.h"
#include "globalvariables.h"
#include "utils.h"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace hooks {

using AddStealItemFunc = game::AddStealItemApi::Api::AddStealItem;

static AddStealItemFunc addStealItemOrig;

static const char scenStealItemValue[]{"STEAL_ITEM_VALUE"};
static constexpr int noStealItemValueLimit{9999};

bool getScenarioVariable(const game::IMidgardObjectMap* objectMap, const char* name, int& value)
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

int getStealItemValueLimit(const game::IMidgardObjectMap* objectMap)
{
    int value{};

    if (getScenarioVariable(objectMap, scenStealItemValue, value)) {
        return std::max(0, value);
    }

    const game::GlobalData* global{*game::GlobalDataApi::get().getGlobalData()};

    const game::GlobalVariables* variables{global->globalVariables};

    return variables->data->stealItemValue;
}

char* __fastcall addStealItemHooked(void* thisptr, void*, game::StealItemEntry* entry)
{
    const auto objectMap{hooks::getObjectMap()};
    const auto limit{getStealItemValueLimit(objectMap)};

    if (limit == noStealItemValueLimit) {
        return addStealItemOrig(thisptr, entry);
    }

    spdlog::info("STEAL ITEM {} gold={} limit={}", idToString(&entry->itemId), entry->value.gold,
                 limit);

    if (entry->value.gold >= limit) {
        spdlog::info("FILTERED {}", idToString(&entry->itemId));

        return reinterpret_cast<char*>(thisptr);
    }

    return addStealItemOrig(thisptr, entry);
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