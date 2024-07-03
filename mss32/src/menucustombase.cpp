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
#include "button.h"
#include "dialoginterf.h"
#include "editboxinterf.h"
#include "interfmanager.h"
#include "log.h"
#include "mempool.h"
#include "menuflashwait.h"
#include "menuphase.h"
#include "midgardmsgbox.h"
#include "netcustomservice.h"
#include "popupdialoginterf.h"
#include "utils.h"

namespace hooks {

CMenuCustomBase::CMenuCustomBase(game::CMenuBase* menu)
    : m_menu{menu}
    , m_menuWait{nullptr}
    , m_peerCallback{this}
    , m_lobbyCallback{this}
{
    auto service = getNetService();
    if (service) {
        service->addPeerCallback(&m_peerCallback);
        service->addLobbyCallback(&m_lobbyCallback);
    }
}

CMenuCustomBase::~CMenuCustomBase()
{
    hideWaitDialog();

    auto service = getNetService();
    if (service) {
        service->removeLobbyCallback(&m_lobbyCallback);
        service->removePeerCallback(&m_peerCallback);
    }
}

game::CMenuBase* CMenuCustomBase::getMenu() const
{
    return m_menu;
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

    hideWaitDialog();

    auto handler = (CMidMsgBoxBackToMainButtonHandler*)Memory::get().allocate(
        sizeof(CMidMsgBoxBackToMainButtonHandler));
    new (handler) CMidMsgBoxBackToMainButtonHandler(m_menu);

    // Connection to the server is lost.
    showMessageBox(getInterfaceText("X005TA0403"), handler, false);
}

void CMenuCustomBase::setAccountNameToEditName()
{
    using namespace game;

    auto dialog = CMenuBaseApi::get().getDialogInterface(getMenu());
    auto editName = CDialogInterfApi::get().findEditBox(dialog, "EDIT_NAME");
    CEditBoxInterfApi::get().setString(editName, getNetService()->getAccountName().c_str());
    editName->data->editable = false;
}

CMenuCustomBase::CPopupDialogCustomBase::CPopupDialogCustomBase(game::CPopupDialogInterf* dialog,
                                                                const char* dialogName)
    : m_dialog{dialog}
    , m_dialogName{dialogName}
{ }

CMenuCustomBase::CMidMsgBoxBackToMainButtonHandler::CMidMsgBoxBackToMainButtonHandler(
    game::CMenuBase* menu)
    : m_menu{menu}
{
    static game::CMidMsgBoxButtonHandlerVftable vftable = {
        (game::CMidMsgBoxButtonHandlerVftable::Destructor)destructor,
        (game::CMidMsgBoxButtonHandlerVftable::Handler)handler,
    };

    this->vftable = &vftable;
}

void __fastcall CMenuCustomBase::CMidMsgBoxBackToMainButtonHandler::destructor(
    CMidMsgBoxBackToMainButtonHandler* thisptr,
    int /*%edx*/,
    char flags)
{ }

void __fastcall CMenuCustomBase::CMidMsgBoxBackToMainButtonHandler::handler(
    CMidMsgBoxBackToMainButtonHandler* thisptr,
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
        CMenuPhaseApi::get().backToMainOrCloseGame(menuPhase, true);
    }
}

void CMenuCustomBase::PeerCallback::onPacketReceived(DefaultMessageIDTypes type,
                                                     SLNet::RakPeerInterface* peer,
                                                     const SLNet::Packet* packet)
{
    switch (type) {
    case ID_DISCONNECTION_NOTIFICATION:
    case ID_CONNECTION_LOST:
        m_menu->onConnectionLost();
        break;
    }
}

void CMenuCustomBase::LobbyCallback::MessageResult(SLNet::Notification_Client_RemoteLogin* message)
{
    if (message->resultCode == SLNet::L2RC_SUCCESS) {
        // We can possibly become logged out by a remote login of the same account.
        // TODO: find a better way (there seems to be no special notification message).
        if (!getNetService()->loggedIn()) {
            m_menu->onConnectionLost();
        }
    }
}

} // namespace hooks
