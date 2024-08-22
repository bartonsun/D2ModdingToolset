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

#ifndef DRAGANDDROPINTERF_H
#define DRAGANDDROPINTERF_H

#include "d2list.h"
#include "fullscreeninterf.h"
#include "middropmanager.h"
#include "mqpoint.h"
#include "smartptr.h"

namespace game {

struct IMidDropSource;
struct IMidDropTarget;
struct CDialogInterf;
struct CInterfManagerImpl;
struct ITask;

struct CDragAndDropInterfData
{
    bool unknown;
    char padding[3];
    IMidDropSource* currentSource;
    char unknown8[4];
    SmartPtr<CInterfManagerImpl> interfManager;
    List<IMidDropSource*> dropSources;
    List<IMidDropTarget*> dropTargets;
    CMqPoint mousePosition;
    CDialogInterf* dialogInterf;
    ITask* task;
    char unknown68[4];
};

assert_size(CDragAndDropInterfData, 72);

struct CDragAndDropInterf : public CFullScreenInterf
{
    IMidDropManager dropManager;
    CDragAndDropInterfData* dragAndDropInterfData;
};

namespace CDragAndDropInterfApi {

struct Api
{
    using GetDialog = CDialogInterf*(__thiscall*)(CDragAndDropInterf* thisptr);
    GetDialog getDialog;

    using Constructor = CDragAndDropInterf*(__thiscall*)(CDragAndDropInterf* thisptr,
                                                         const char* dialogName,
                                                         ITask* task,
                                                         bool a8);
    Constructor constructor;
};

Api& get();

} // namespace CDragAndDropInterfApi

} // namespace game

#endif // DRAGANDDROPINTERF_H
