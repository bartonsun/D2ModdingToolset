/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Vladimir Makeev.
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

#ifndef IMAGELAYERLIST_H
#define IMAGELAYERLIST_H

#include "d2list.h"
#include "d2pair.h"
#include "idset.h"

namespace game {

struct IMqImage2;
struct CIsoLayer;
struct CFortification;
struct IMidgardObjectMap;
struct CMidgardID;
struct IMapElement;

using ImageLayerPair = Pair<IMqImage2*, const CIsoLayer*>;
using ImageLayerList = List<ImageLayerPair>;

namespace ImageLayerListApi {

struct Api
{
    /** Adds pair to the end of the list. */
    using PushBack = void(__thiscall*)(ImageLayerList* thisptr, const ImageLayerPair* pair);
    PushBack pushBack;

    /** Clears list contents. */
    using Clear = void(__thiscall*)(ImageLayerList* thisptr);
    Clear clear;

    /**
     * Adds shield image and iso layer pair to the list.
     * Shield image is shown above city entrance when there is stack guarding it.
     */
    using AddShieldImageLayer = void(__stdcall*)(ImageLayerList* list,
                                                 const CFortification* fortification,
                                                 int capital,
                                                 const IMidgardObjectMap* objectMap,
                                                 const CMidgardID* playerId);
    AddShieldImageLayer addShieldImageLayer;

    /**
     * Adds images and iso layers to the list depending on specified map element.
     * Images describe how map element should look on each layer on a strategic map.
     * @param list - list of images and corresponding iso layers.
     * @param mapElement - map element to show on a strategic map.
     * @param objectMap - objects storage.
     * @param playerId - id of current player.
     * @param objectives - list of scenario objective ids.
     * @param unknown
     * @param animatedIso - whether strategic map should show animated graphics.
     */
    using GetMapElementIsoLayerImages = void(__stdcall*)(ImageLayerList* list,
                                                         const IMapElement* mapElement,
                                                         const IMidgardObjectMap* objectMap,
                                                         const CMidgardID* playerId,
                                                         const IdSet* objectives,
                                                         int unknown,
                                                         bool animatedIso);
    GetMapElementIsoLayerImages getMapElementIsoLayerImages;
};

Api& get();

} // namespace ImageLayerListApi

} // namespace game

#endif // IMAGELAYERLIST_H
