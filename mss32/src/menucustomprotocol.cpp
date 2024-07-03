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

#include "menucustomprotocol.h"
#include "dialoginterf.h"
#include "editboxinterf.h"
#include "interfaceutils.h"
#include "listbox.h"
#include "log.h"
#include "mempool.h"
#include "menuflashwait.h"
#include "menuphase.h"
#include "midgard.h"
#include "restrictions.h"
#include "textids.h"
#include "utils.h"
#include <fmt/format.h>

namespace hooks {

CMenuCustomProtocol::CMenuCustomProtocol(game::CMenuPhase* menuPhase)
    : CMenuCustomBase{this}
    , m_peerCallback{this}
    , m_lobbyCallback{this}
    , m_loginDialog{nullptr}
    , m_registerDialog{nullptr}
{
    using namespace game;

    CMenuProtocolApi::get().constructor(this, menuPhase);

    static RttiInfo<CMenuBaseVftable> rttiInfo = {};
    if (rttiInfo.locator == nullptr) {
        replaceRttiInfo(rttiInfo, this->vftable);
        rttiInfo.vftable.destructor = (CInterfaceVftable::Destructor)&destructor;
    }
    this->vftable = &rttiInfo.vftable;

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBox = CDialogInterfApi::get().findListBox(dialog, "TLBOX_PROTOCOL");
    if (listBox) {
        // One more entry for our custom protocol
        CListBoxInterfApi::get().setElementsTotal(listBox, listBox->listBoxData->elementsTotal + 1);
    }
}

CMenuCustomProtocol ::~CMenuCustomProtocol()
{
    using namespace game;

    hideLoginDialog();
    hideRegisterDialog();

    auto service = getNetService();
    if (service) {
        service->removeLobbyCallback(&m_lobbyCallback);
        service->removePeerCallback(&m_peerCallback);
    }

    CMenuProtocolApi::get().destructor(this);
}

void __fastcall CMenuCustomProtocol::destructor(CMenuCustomProtocol* thisptr,
                                                int /*%edx*/,
                                                char flags)
{
    thisptr->~CMenuCustomProtocol();

    if (flags & 1) {
        logDebug("transitions.log", "Free CMenuCustomProtocol memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

void CMenuCustomProtocol::createNetCustomServiceStartWaitingConnection()
{
    using namespace game;

    const auto& midgardApi = CMidgardApi::get();

    auto service = createNetCustomServiceStartConnection();
    if (!service) {
        auto message{getInterfaceText(textIds().lobby.connectStartFailed.c_str())};
        if (message.empty()) {
            message = "Connection start failed";
        }
        return;
    }
    service->addPeerCallback(&m_peerCallback);
    service->addLobbyCallback(&m_lobbyCallback);
    midgardApi.setNetService(midgardApi.instance(), service, true, false);

    showWaitDialog();
}

void CMenuCustomProtocol::showLoginDialog()
{
    using namespace game;

    if (m_loginDialog) {
        logDebug("lobby.log", "Error showing login dialog that is already shown");
        return;
    }

    m_loginDialog = (CLoginAccountInterf*)game::Memory::get().allocate(sizeof(CLoginAccountInterf));
    showInterface(new (m_loginDialog) CLoginAccountInterf(this));
}

void CMenuCustomProtocol::hideLoginDialog()
{
    if (m_loginDialog) {
        hideInterface(m_loginDialog);
        m_loginDialog->vftable->destructor(m_loginDialog, 1);
        m_loginDialog = nullptr;
    }
}

void CMenuCustomProtocol::showRegisterDialog()
{
    using namespace game;

    if (m_registerDialog) {
        logDebug("lobby.log", "Error showing register dialog that is already shown");
        return;
    }

    m_registerDialog = (CRegisterAccountInterf*)game::Memory::get().allocate(
        sizeof(CRegisterAccountInterf));
    showInterface(new (m_registerDialog) CRegisterAccountInterf(this));
}

void CMenuCustomProtocol::hideRegisterDialog()
{
    if (m_registerDialog) {
        hideInterface(m_registerDialog);
        m_registerDialog->vftable->destructor(m_registerDialog, 1);
        m_registerDialog = nullptr;
    }
}

void CMenuCustomProtocol::PeerCallback::onPacketReceived(DefaultMessageIDTypes type,
                                                         SLNet::RakPeerInterface* peer,
                                                         const SLNet::Packet* packet)
{
    using namespace game;

    switch (type) {
    case ID_CONNECTION_REQUEST_ACCEPTED:
        m_menu->hideWaitDialog();
        m_menu->showLoginDialog();
        break;

    case ID_CONNECTION_ATTEMPT_FAILED: {
        m_menu->hideWaitDialog();

        auto message{getInterfaceText(textIds().lobby.connectAttemptFailed.c_str())};
        if (message.empty()) {
            message = "Connection attempt failed";
        }
        showMessageBox(message);
        break;
    }

    case ID_NO_FREE_INCOMING_CONNECTIONS: {
        m_menu->hideWaitDialog();

        auto message{getInterfaceText(textIds().lobby.serverIsFull.c_str())};
        if (message.empty()) {
            message = "Lobby server is full";
        }
        showMessageBox(message);
        break;
    }

    case ID_DISCONNECTION_NOTIFICATION:
    case ID_CONNECTION_LOST:
        // We need to handle this ourself: the CMenuCustomBase callback is not attached because the
        // service is not there yet on the menu creation
        m_menu->onConnectionLost();
        break;
    }
}

void CMenuCustomProtocol::LobbyCallback::MessageResult(SLNet::Client_Login* message)
{
    using namespace game;

    m_menu->hideWaitDialog();

    switch (message->resultCode) {
    case SLNet::L2RC_SUCCESS: {
        m_menu->hideLoginDialog();

        CMenuPhase* menuPhase = m_menu->menuBaseData->menuPhase;
        CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::Protocol2CustomLobby);
        break;
    }

    case SLNet::L2RC_Client_Login_HANDLE_NOT_IN_USE_OR_BAD_SECRET_KEY: {
        auto msg{getInterfaceText(textIds().lobby.noSuchAccountOrWrongPassword.c_str())};
        if (msg.empty()) {
            msg = "Wrong password or the account does not exist.";
        }
        showMessageBox(msg);
        break;
    }

    case SLNet::L2RC_Client_Login_BANNED: {
        auto msg{getInterfaceText(textIds().lobby.accountIsBanned.c_str())};
        if (msg.empty()) {
            msg = "The account is banned from the lobby server.";
        }
        showMessageBox(msg);
        break;
    }

    default: {
        auto msg{getInterfaceText(textIds().lobby.unableToLogin.c_str())};
        if (msg.empty()) {
            msg = "An unexpected error during login.\nError code: %CODE%.";
        }
        // TODO: Lobby2ResultCodeDescription::ToEnglish
        replace(msg, "%CODE%", fmt::format("{:d}", message->resultCode));
        showMessageBox(msg);
        break;
    }
    }
}

void CMenuCustomProtocol::LobbyCallback::MessageResult(SLNet::Client_RegisterAccount* message)
{
    using namespace game;

    m_menu->hideWaitDialog();

    switch (message->resultCode) {
    case SLNet::L2RC_SUCCESS: {
        m_menu->hideRegisterDialog();

        if (!getNetService()->loginAccount(message->userName,
                                           message->createAccountParameters.password)) {
            auto msg{getInterfaceText(textIds().lobby.unableToLoginAfterRegistration.c_str())};
            if (msg.empty()) {
                msg = "An unexpected error trying to login after successful registration.";
            }
            showMessageBox(msg);
            break;
        }

        m_menu->showWaitDialog();
        break;
    }

    case SLNet::L2RC_Client_RegisterAccount_HANDLE_ALREADY_IN_USE: {
        auto msg{getInterfaceText(textIds().lobby.accountNameAlreadyInUse.c_str())};
        if (msg.empty()) {
            msg = "The account name already exists.";
        }
        showMessageBox(msg);
        break;
    }

    default: {
        auto msg{getInterfaceText(textIds().lobby.unableToRegister.c_str())};
        if (msg.empty()) {
            msg = "An unexpected error during account registration.\nError code: %CODE%.";
        }
        // TODO: Lobby2ResultCodeDescription::ToEnglish
        replace(msg, "%CODE%", fmt::format("{:d}", message->resultCode));
        showMessageBox(msg);
        break;
    }
    }
}

CMenuCustomProtocol::CLoginAccountInterf::CLoginAccountInterf(CMenuCustomProtocol* menu,
                                                              const char* dialogName)
    : CPopupDialogCustomBase{this, dialogName}
    , m_menu{menu}
{
    using namespace game;

    const auto& restrictions = gameRestrictions();

    CPopupDialogInterfApi::get().constructor(this, dialogName, nullptr);

    setButtonCallback(*dialog, "BTN_CANCEL", cancelBtnHandler, this);
    setButtonCallback(*dialog, "BTN_OK", okBtnHandler, this);
    setButtonCallback(*dialog, "BTN_REGISTER", registerBtnHandler, this);

    // Using EditFilter::Names for consistency with other game menus like CMenuNewSkirmishMulti.
    // Account name shares player name restrictions as they are interchangeable (at least for now).
    setEditBoxData(*dialog, "EDIT_ACCOUNT_NAME", EditFilter::Names,
                   *restrictions.playerNameMaxLength, false);
    setEditBoxData(*dialog, "EDIT_PASSWORD", EditFilter::Names,
                   CNetCustomService::passwordMaxLength, true);
}

void __fastcall CMenuCustomProtocol::CLoginAccountInterf::okBtnHandler(CLoginAccountInterf* thisptr,
                                                                       int /*%edx*/)
{
    auto dialog = *thisptr->dialog;
    if (!getNetService()->loginAccount(getEditBoxText(dialog, "EDIT_ACCOUNT_NAME"),
                                       getEditBoxText(dialog, "EDIT_PASSWORD"))) {
        auto message{getInterfaceText(textIds().lobby.invalidAccountNameOrPassword.c_str())};
        if (message.empty()) {
            message = "Account name or password are either empty or invalid.";
        }
        showMessageBox(message);
        return;
    }

    thisptr->m_menu->showWaitDialog();
}

void __fastcall CMenuCustomProtocol::CLoginAccountInterf::cancelBtnHandler(
    CLoginAccountInterf* thisptr,
    int /*%edx*/)
{
    logDebug("lobby.log", "User canceled logging in");
    thisptr->m_menu->hideLoginDialog();
}

void __fastcall CMenuCustomProtocol::CLoginAccountInterf::registerBtnHandler(
    CLoginAccountInterf* thisptr,
    int /*%edx*/)
{
    auto menu = thisptr->m_menu;
    menu->hideLoginDialog();
    menu->showRegisterDialog();
}

CMenuCustomProtocol::CRegisterAccountInterf::CRegisterAccountInterf(CMenuCustomProtocol* menu,
                                                                    const char* dialogName)
    : CPopupDialogCustomBase{this, dialogName}
    , m_menu{menu}
{
    using namespace game;

    const auto& restrictions = gameRestrictions();

    CPopupDialogInterfApi::get().constructor(this, dialogName, nullptr);

    setButtonCallback(*dialog, "BTN_CANCEL", cancelBtnHandler, this);
    setButtonCallback(*dialog, "BTN_OK", okBtnHandler, this);

    // Using EditFilter::Names for consistency with other game menus like CMenuNewSkirmishMulti.
    // Account name shares player name restrictions as they are interchangeable (at least for now).
    setEditBoxData(*dialog, "EDIT_ACCOUNT_NAME", EditFilter::Names,
                   *restrictions.playerNameMaxLength, false);
    // TODO: add repeat-password edit, mask both edits with asterisks
    setEditBoxData(*dialog, "EDIT_PASSWORD", EditFilter::Names,
                   CNetCustomService::passwordMaxLength, false);
}

void __fastcall CMenuCustomProtocol::CRegisterAccountInterf::okBtnHandler(
    CRegisterAccountInterf* thisptr,
    int /*%edx*/)
{
    auto dialog = *thisptr->dialog;
    if (!getNetService()->createAccount(getEditBoxText(dialog, "EDIT_ACCOUNT_NAME"),
                                        getEditBoxText(dialog, "EDIT_PASSWORD"))) {
        auto message{getInterfaceText(textIds().lobby.invalidAccountNameOrPassword.c_str())};
        if (message.empty()) {
            message = "Account name or password are either empty or invalid.";
        }
        showMessageBox(message);
        return;
    }

    thisptr->m_menu->showWaitDialog();
}

void __fastcall CMenuCustomProtocol::CRegisterAccountInterf::cancelBtnHandler(
    CRegisterAccountInterf* thisptr,
    int /*%edx*/)
{
    logDebug("lobby.log", "User canceled register account");
    thisptr->m_menu->hideRegisterDialog();
}

} // namespace hooks
