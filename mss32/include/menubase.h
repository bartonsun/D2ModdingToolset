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

#ifndef MENUBASE_H
#define MENUBASE_H

#include "imagepointlist.h"
#include "interffullscreen.h"
#include "smartptr.h"
#include "uievent.h"
#include <cstddef>

namespace game {

struct CDialogInterf;
struct CMenuPhase;
struct CSpinButtonInterf;

struct CMenuBaseData
{
    CMenuPhase* menuPhase;
    CDialogInterf* menuDialog;
    UiEvent uiEvent;
    SmartPtr<IMqImage2> transitionAnimation;
    CMqPoint animationCenter;
};

assert_size(CMenuBaseData, 48);

/** Base class for all menus. */
struct CMenuBase : public CInterfFullScreen
{
    CMenuBaseData* menuBaseData;
};

assert_size(CMenuBase, 12);
assert_offset(CMenuBase, menuBaseData, 8);

namespace CMenuBaseApi {

struct Api
{
    /**
     * Initializes CMenuBase object, sets vftable.
     * @param[in] thisptr pointer to existing CMenuBase object.
     * @param a1 meaning unknown.
     * @returns thisptr.
     */
    using Constructor = CMenuBase*(__thiscall*)(CMenuBase* thisptr, CMenuPhase* menuPhase);
    Constructor constructor;

    /** Creates ui elements of specified dialog to be used in menu. */
    using CreateMenu = int(__thiscall*)(CMenuBase* thisptr, const char* dialogName);
    CreateMenu createMenu;

    /** Returns CDialogInterf object. */
    using GetDialogInterface = CDialogInterf*(__thiscall*)(CMenuBase* thisptr);
    GetDialogInterface getDialogInterface;

    /** Returns back to main menu and clears network logic. */
    using ButtonCallback = void(__thiscall*)(CMenuBase* thisptr);
    ButtonCallback buttonBackCallback;

    /**
     * Creates functor for buttons of CMenuBase and its childs.
     * Reused from CMenuNewSkirmish.
     */
    using CreateButtonFunctor = SmartPointer*(__stdcall*)(SmartPointer* functor,
                                                          int a2,
                                                          CMenuBase* menu,
                                                          const ButtonCallback* callback);
    CreateButtonFunctor createButtonFunctor;

    using ListBoxDisplayCallback = void(__thiscall*)(CMenuBase* thisptr,
                                                     ImagePointList* contents,
                                                     const CMqRect* lineArea,
                                                     int index,
                                                     bool selected);

    using CreateListBoxDisplayFunctor = SmartPointer*(__stdcall*)(SmartPointer* functor,
                                                                  int a2,
                                                                  CMenuBase* menu,
                                                                  ListBoxDisplayCallback* callback);
    CreateListBoxDisplayFunctor createListBoxDisplayFunctor;

    using SpinButtonCallback = void(__thiscall*)(CMenuBase* thisptr, CSpinButtonInterf* spinButton);

    /**
     * Creates functor for spin buttons of CMenuBase and its childs.
     * Reused from CMenuLobbyHost.
     */
    using CreateSpinButtonFunctor = SmartPointer*(__stdcall*)(SmartPointer* functor,
                                                              int a2,
                                                              CMenuBase* menu,
                                                              const SpinButtonCallback* callback);
    CreateSpinButtonFunctor createSpinButtonFunctor;
};

Api& get();

} // namespace CMenuBaseApi

} // namespace game

#endif // MENUBASE_H
