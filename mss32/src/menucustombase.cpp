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

#include "menucustombase.h"
#include "interfmanager.h"
#include "log.h"
#include "mempool.h"
#include "menubase.h"
#include "menuflashwait.h"
#include "midgardmsgbox.h"
#include "originalfunctions.h"
#include "utils.h"

namespace hooks {

CMenuCustomBase::CMenuCustomBase(game::CMenuBase* menu)
    : m_menu{menu}
    , m_menuWait{nullptr}
{ }

CMenuCustomBase::~CMenuCustomBase()
{
    hideWaitDialog();
}

void CMenuCustomBase::showWaitDialog()
{
    using namespace game;

    if (m_menuWait) {
        logDebug("lobby.log", "Error showing wait dialog that is already shown");
        return;
    }

    m_menuWait = (CMenuFlashWait*)Memory::get().allocate(sizeof(CMenuFlashWait));
    CMenuFlashWaitApi::get().constructor(m_menuWait);
    showInterface(m_menuWait);
}

void CMenuCustomBase::hideWaitDialog()
{
    if (m_menuWait) {
        hideInterface(m_menuWait);
        m_menuWait->vftable->destructor(m_menuWait, 1);
        m_menuWait = nullptr;
    }
}

void CMenuCustomBase::onConnectionLost()
{
    using namespace game;

    auto handler = (CConnectionLostMsgBoxButtonHandler*)Memory::get().allocate(
        sizeof(CConnectionLostMsgBoxButtonHandler));
    new (handler) CConnectionLostMsgBoxButtonHandler(m_menu);

    // Connection to the server is lost.
    showMessageBox(getInterfaceText("X005TA0403"), handler, false);
}

CMenuCustomBase::CConnectionLostMsgBoxButtonHandler::CConnectionLostMsgBoxButtonHandler(
    game::CMenuBase* menu)
    : m_menu{menu}
{
    static game::CMidMsgBoxButtonHandlerVftable vftable = {
        (game::CMidMsgBoxButtonHandlerVftable::Destructor)destructor,
        (game::CMidMsgBoxButtonHandlerVftable::Handler)handler,
    };

    this->vftable = &vftable;
}

void __fastcall CMenuCustomBase::CConnectionLostMsgBoxButtonHandler::destructor(
    CConnectionLostMsgBoxButtonHandler* thisptr,
    int /*%edx*/,
    char flags)
{ }

void __fastcall CMenuCustomBase::CConnectionLostMsgBoxButtonHandler::handler(
    CConnectionLostMsgBoxButtonHandler* thisptr,
    int /*%edx*/,
    game::CMidgardMsgBox* msgBox,
    bool okPressed)
{
    using namespace game;

    hideInterface(msgBox);
    if (msgBox) {
        msgBox->vftable->destructor(msgBox, 1);
    }

    if (okPressed) {
        auto menuPhase = thisptr->m_menu->menuBaseData->menuPhase;
        getOriginalFunctions().menuPhaseBackToMainOrCloseGame(menuPhase, true);
    }
}

} // namespace hooks