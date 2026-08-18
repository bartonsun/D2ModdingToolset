/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2026
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
 */

#include "bagdroptombhooks.h"
#include "midgardid.h"
#include "midgardplan.h"
#include "originalfunctions.h"
#include "version.h"
#include <cstdint>
#include <intrin.h>

namespace hooks {

namespace {

bool isDropBagObjectsLookup(std::uintptr_t returnAddress)
{
    switch (gameVersion()) {
    case GameVersion::Akella:
    case GameVersion::Russobit:
        return returnAddress == 0x4cfc44 || returnAddress == 0x6119af;
    case GameVersion::Gog:
        return returnAddress == 0x4cf2c6 || returnAddress == 0x6104d4;
    default:
        return false;
    }
}

void removeTombIds(game::IdList* objectIds)
{
    const auto& idApi = game::CMidgardIDApi::get();
    const auto& listApi = game::IdListApi::get();

    for (auto it = objectIds->begin(); it != objectIds->end();) {
        const auto current = it++;
        if (idApi.getType(&*current) == game::IdType::Tomb) {
            listApi.erase(objectIds, current);
        }
    }
}

} // namespace

bool __fastcall getObjectsAtPointHooked(const game::CMidgardPlan* thisptr,
                                        int /*%edx*/,
                                        game::IdList* objectIds,
                                        const game::CMqPoint* mapPosition)
{
    const auto returnAddress = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const bool result =
        getOriginalFunctions().getObjectsAtPoint(thisptr, objectIds, mapPosition);

    if (result && objectIds && isDropBagObjectsLookup(returnAddress)) {
        removeTombIds(objectIds);
    }

    return result;
}

} // namespace hooks
