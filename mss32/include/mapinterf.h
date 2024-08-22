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

#ifndef MAPINTERF_H
#define MAPINTERF_H

#include "isoview.h"

namespace game {
struct CToggleButton;

namespace editor {

struct CTaskMapChange;

/** Actions correspond to toggle buttons of RAD_TERRAIN, DLG_MAP in ScenEdit.dlg. */
enum class MapChangeAction : int
{
    Erase,               /**< TOG_ERASE */
    ColorTerrainHeretic, /**< TOG_TER_HERETIC */
    ColorTerrainDwarf,   /**< TOG_TER_DWARF */
    ColorTerrainHuman,   /**< TOG_TER_HUMAN */
    ColorTerrainUndead,  /**< TOG_TER_UNDEAD */
    PlaceNeutralTerrain, /**< TOG_TER_NEUTRAL */
    AddMountains,        /**< TOG_TER_MOUNT_NE */
    AddTrees,            /**< TOG_TREE */
    PlaceWater,          /**< TOG_TER_WATER */
    ColorTerrainElf,     /**< TOG_TER_ELF */
};

struct CMapInterfData
{
    MapChangeAction selectedAction;
};

assert_size(CMapInterfData, 4);

/** Represents DLG_MAP from ScenEdit.dlg. */
struct CMapInterf : public CIsoView
{
    CMapInterfData* data;
};

assert_size(CMapInterf, 24);
assert_offset(CMapInterf, taskManagerHolder, 8);

static inline CMapInterf* castTaskManagerToMapInterf(ITaskManagerHolder* taskManager)
{
    return reinterpret_cast<CMapInterf*>((std::uintptr_t)taskManager
                                         - offsetof(CMapInterf, taskManagerHolder));
}

namespace CMapInterfApi {

struct Api
{
    /** Creates task for map editing depending on RAD_TERRAIN button selection. */
    using CreateMapChangeTask = CTaskMapChange*(__thiscall*)(ITaskManagerHolder* thisptr);
    CreateMapChangeTask createMapChangeTask;

    using CreateTask = CTaskMapChange*(__stdcall*)(CMapInterf* mapInterf);
    CreateTask createDelMapObjectTask;
    CreateTask createAddMountainsTask;
    CreateTask createAddTreesTask;
    CreateTask createChangeHeightTask;

    struct ToggleButtonCallback
    {
        using Callback = void(__thiscall*)(CMapInterf* thisptr, bool, CToggleButton*);

        Callback callback;
        int unknown;
    };

    using CreateToggleButtonFunctor = SmartPointer*(__stdcall*)(SmartPointer* functor,
                                                                int a2,
                                                                CMapInterf* mapInterf,
                                                                ToggleButtonCallback* callback);
    CreateToggleButtonFunctor createToggleButtonFunctor;
};

Api& get();

} // namespace CMapInterfApi

} // namespace editor
} // namespace game

#endif // MAPINTERF_H
