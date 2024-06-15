/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Stanislav Egorov.
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

#ifndef MENUCUSTOMBASE_H
#define MENUCUSTOMBASE_H

#include "menubase.h"
#include "midmsgboxbuttonhandler.h"
#include <string>

namespace game {
struct CPopupDialogInterf;
struct CMenuFlashWait;
} // namespace game

namespace hooks {

class CMenuCustomBase
{
public:
    CMenuCustomBase(game::CMenuBase* menu);
    ~CMenuCustomBase();

protected:
    game::CMenuBase* getMenu() const;
    void showWaitDialog();
    void hideWaitDialog();
    void onConnectionLost();

    class CPopupDialogCustomBase
    {
    public:
        CPopupDialogCustomBase(game::CPopupDialogInterf* dialog, const char* dialogName);

    protected:
        void assignButtonHandler(const char* buttonName,
                                 game::CMenuBaseApi::Api::ButtonCallback handler);

    private:
        game::CPopupDialogInterf* m_dialog;
        std::string m_dialogName;
    };

    class CConnectionLostMsgBoxButtonHandler : public game::CMidMsgBoxButtonHandler
    {
    public:
        CConnectionLostMsgBoxButtonHandler(game::CMenuBase* menu);

    protected:
        static void __fastcall destructor(CConnectionLostMsgBoxButtonHandler* thisptr,
                                          int /*%edx*/,
                                          char flags);
        static void __fastcall handler(CConnectionLostMsgBoxButtonHandler* thisptr,
                                       int /*%edx*/,
                                       game::CMidgardMsgBox* msgBox,
                                       bool okPressed);

    private:
        game::CMenuBase* m_menu;
    };
    assert_offset(CConnectionLostMsgBoxButtonHandler, vftable, 0);

private:
    game::CMenuBase* m_menu;
    game::CMenuFlashWait* m_menuWait;
};

} // namespace hooks

#endif // MENUCUSTOMBASE_H
