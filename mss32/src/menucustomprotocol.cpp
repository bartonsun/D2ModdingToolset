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
#include "button.h"
#include "dialoginterf.h"
#include "editboxinterf.h"
#include "listbox.h"
#include "log.h"
#include "mempool.h"
#include "menuflashwait.h"
#include "menuphase.h"
#include "midgard.h"
#include "textids.h"
#include "utils.h"
#include <fmt/format.h>

namespace hooks {

CMenuCustomProtocol::CMenuCustomProtocol(game::CMenuPhase* menuPhase)
    : CMenuCustomBase{this}
    , m_peerCallback{this}
    , m_lobbyCallback{this}
    , m_loginDialog{nullptr}
{
    using namespace game;

    CMenuProtocolApi::get().constructor(this, menuPhase);

    static game::RttiInfo<game::CMenuBaseVftable> rttiInfo = {};
    if (rttiInfo.locator == nullptr) {
        replaceRttiInfo(rttiInfo, this->vftable);
        rttiInfo.vftable.destructor = (game::CInterfaceVftable::Destructor)&destructor;
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

    hideWaitDialog();
    hideLoginDialog();

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

void CMenuCustomProtocol::PeerCallback::onPacketReceived(DefaultMessageIDTypes type,
                                                         SLNet::RakPeerInterface* peer,
                                                         const SLNet::Packet* packet)
{
    using namespace game;

    switch (type) {
    case ID_CONNECTION_ATTEMPT_FAILED: {
        auto message{getInterfaceText(textIds().lobby.connectAttemptFailed.c_str())};
        if (message.empty()) {
            message = "Connection attempt failed";
        }

        m_menu->hideWaitDialog();
        showMessageBox(message);
        break;
    }

    case ID_NO_FREE_INCOMING_CONNECTIONS: {
        auto message{getInterfaceText(textIds().lobby.serverIsFull.c_str())};
        if (message.empty()) {
            message = "Lobby server is full";
        }

        m_menu->hideWaitDialog();
        showMessageBox(message);
        break;
    }

    case ID_ALREADY_CONNECTED:
        m_menu->hideWaitDialog();
        logDebug("lobby.log", "Error connecting - already connected. This should never happen.");
        break;

    // TODO: move integrity request to custom service callbacks, only check results here
    case ID_CONNECTION_REQUEST_ACCEPTED: {
        std::string hash;
        if (!computeHash(globalsFolder(), hash)) {
            auto message{getInterfaceText(textIds().lobby.computeHashFailed.c_str())};
            if (message.empty()) {
                message = "Could not compute hash";
            }

            m_menu->hideWaitDialog();
            showMessageBox(message);
            break;
        }

        if (!getNetService()->checkFilesIntegrity(hash.c_str())) {
            auto message{getInterfaceText(textIds().lobby.requestHashCheckFailed.c_str())};
            if (message.empty()) {
                message = "Could not request game integrity check";
            }

            m_menu->hideWaitDialog();
            showMessageBox(message);
            break;
        }

        return;
    }

    case ID_FILES_INTEGRITY_RESULT: {
        m_menu->hideWaitDialog();

        SLNet::BitStream input{packet->data, packet->length, false};
        input.IgnoreBytes(sizeof(SLNet::MessageID));

        bool checkPassed{false};
        input.Read(checkPassed);
        if (!checkPassed) {
            auto message{getInterfaceText(textIds().lobby.wrongHash.c_str())};
            if (message.empty()) {
                message = "Game integrity check failed";
            }

            showMessageBox(message);
            break;
        }

        m_menu->showLoginDialog();
        break;
    }

    case ID_DISCONNECTION_NOTIFICATION: {
        logDebug("lobby.log", "Server was shut down");
        m_menu->onConnectionLost();
        break;
    }

    case ID_CONNECTION_LOST: {
        logDebug("lobby.log", "Connection with server is lost");
        m_menu->onConnectionLost();
        break;
    }
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
        replace(msg, "%CODE%", fmt::format("{:d}", message->resultCode));
        showMessageBox(msg);
        break;
    }
    }
}

CMenuCustomProtocol::CLoginAccountInterf::CLoginAccountInterf(CMenuCustomProtocol* menu)
    : m_menu{menu}
{
    using namespace game;

    const auto createFunctor = CMenuBaseApi::get().createButtonFunctor;
    const auto assignFunctor = CButtonInterfApi::get().assignFunctor;
    const auto freeFunctor = SmartPointerApi::get().createOrFreeNoDtor;

    CPopupDialogInterfApi::get().constructor(this, loginDialogName, nullptr);

    SmartPointer functor;
    auto callback = (CMenuBaseApi::Api::ButtonCallback)cancelBtnHandler;
    createFunctor(&functor, 0, (CMenuBase*)this, &callback);
    assignFunctor(*this->dialog, "BTN_CANCEL", loginDialogName, &functor, 0);
    freeFunctor(&functor, nullptr);

    callback = (CMenuBaseApi::Api::ButtonCallback)okBtnHandler;
    createFunctor(&functor, 0, (CMenuBase*)this, &callback);
    assignFunctor(*this->dialog, "BTN_OK", loginDialogName, &functor, 0);
    freeFunctor(&functor, nullptr);
}

void __fastcall CMenuCustomProtocol::CLoginAccountInterf::okBtnHandler(CLoginAccountInterf* thisptr,
                                                                       int /*%edx*/)
{
    using namespace game;

    auto& dialogApi = CDialogInterfApi::get();
    auto dialog = *thisptr->dialog;

    const char* accountName = nullptr;
    auto accountNameEdit = dialogApi.findEditBox(dialog, "EDIT_ACCOUNT_NAME");
    if (accountNameEdit) {
        accountName = accountNameEdit->data->editBoxData.inputString.string;
    }

    const char* password = nullptr;
    auto passwordEdit = dialogApi.findEditBox(dialog, "EDIT_PASSWORD");
    if (passwordEdit) {
        password = passwordEdit->data->editBoxData.inputString.string;
    }

    if (!getNetService()->loginAccount(accountName, password)) {
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

} // namespace hooks
