/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2020 Vladimir Makeev.
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

#include "editor.h"

namespace game {

// clang-format off
EditorFunctions editorFunctions{
    (RadioButtonIndexToPlayerId)0x429f1b,
    (FindPlayerByRaceCategory)0x4e1bf5,
    (CanPlace)0x511142,
    (CanPlace)0x512376,
    (CountStacksOnMap)0x40b631,
    (GetSubRaceByRace)0x50b1a6,
    (IsRaceCategoryPlayable)0x419193,
    (ChangeCapitalTerrain)0x50afb4,
    (GetObjectNamePos)0x45797c,
    (GetSiteImageIndices)0x556835,
    (GetSiteImage)0x5569f4,
    (GetSiteAtPosition)0x410cf7,
    (ShowOrHideSiteOnStrategicMap)0x553907,
};
// clang-format on

} // namespace game
