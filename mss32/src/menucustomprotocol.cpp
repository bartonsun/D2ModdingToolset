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
#include "listbox.h"
#include "log.h"
#include "loginaccountinterf.h"
#include "mempool.h"
#include "menuflashwait.h"
#include "menuphase.h"
#include "midgard.h"
#include "textids.h"
#include "utils.h"

namespace hooks {

CMenuCustomProtocol::CMenuCustomProtocol(game::CMenuPhase* menuPhase)
    : m_menuWait{nullptr}
    , m_callbacks{this}
    , m_lobbyCallbacks{this}
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

    stopWaitingConnection();

    auto service = getNetService();
    if (service) {
        service->removeLobbyCallbacks(&m_lobbyCallbacks);
        service->removePeerCallbacks(&m_callbacks);
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

    stopWaitingConnection();

    auto midgard = midgardApi.instance();
    auto service = CNetCustomService::create();
    midgardApi.setNetService(midgard, service, true, false);
    if (!service) {
        // TODO: show error dialog
        return;
    }
    service->addPeerCallbacks(&m_callbacks);
    service->addLobbyCallbacks(&m_lobbyCallbacks);

    m_menuWait = (CMenuFlashWait*)Memory::get().allocate(sizeof(CMenuFlashWait));
    CMenuFlashWaitApi::get().constructor(m_menuWait);
    showInterface(m_menuWait);
}

void CMenuCustomProtocol::stopWaitingConnection()
{
    using namespace game;

    if (m_menuWait) {
        hideInterface(m_menuWait);
        m_menuWait->vftable->destructor(m_menuWait, 1);
        m_menuWait = nullptr;
    }
}

void CMenuCustomProtocol::stopWaitingConnection(const char* errorMessage)
{
    stopWaitingConnection();
    showMessageBox(errorMessage);
}

void CMenuCustomProtocol::Callbacks::onPacketReceived(DefaultMessageIDTypes type,
                                                      SLNet::RakPeerInterface* peer,
                                                      const SLNet::Packet* packet)
{
    switch (type) {
    case ID_CONNECTION_ATTEMPT_FAILED: {
        auto message{getInterfaceText(textIds().lobby.connectAttemptFailed.c_str())};
        if (message.empty()) {
            message = "Connection attempt failed";
        }

        m_menu->stopWaitingConnection(message.c_str());
        break;
    }
    case ID_NO_FREE_INCOMING_CONNECTIONS: {
        auto message{getInterfaceText(textIds().lobby.serverIsFull.c_str())};
        if (message.empty()) {
            message = "Lobby server is full";
        }

        m_menu->stopWaitingConnection(message.c_str());
        break;
    }
    case ID_ALREADY_CONNECTED:
        m_menu->stopWaitingConnection("Already connected.\nThis should never happen");
        break;

    // TODO: move integrity request to custom service callbacks, only check results here
    case ID_CONNECTION_REQUEST_ACCEPTED: {
        std::string hash;
        if (!computeHash(globalsFolder(), hash)) {
            auto message{getInterfaceText(textIds().lobby.computeHashFailed.c_str())};
            if (message.empty()) {
                message = "Could not compute hash";
            }

            m_menu->stopWaitingConnection(message.c_str());
            break;
        }

        if (!getNetService()->checkFilesIntegrity(hash.c_str())) {
            auto message{getInterfaceText(textIds().lobby.requestHashCheckFailed.c_str())};
            if (message.empty()) {
                message = "Could not request game integrity check";
            }

            m_menu->stopWaitingConnection(message.c_str());
            break;
        }

        return;
    }

    case ID_FILES_INTEGRITY_RESULT: {
        m_menu->stopWaitingConnection();

        SLNet::BitStream input{packet->data, packet->length, false};
        input.IgnoreBytes(sizeof(SLNet::MessageID));

        bool checkPassed{false};
        input.Read(checkPassed);
        if (!checkPassed) {
            auto message{getInterfaceText(textIds().lobby.wrongHash.c_str())};
            if (message.empty()) {
                message = "Game integrity check failed";
            }

            m_menu->stopWaitingConnection(message.c_str());
            break;
        }

        // TODO: refactor, control wait message
        showLoginAccountDialog();
        break;
    }
    }
}

void CMenuCustomProtocol::LobbyCallbacks::MessageResult(SLNet::Client_Login* message)
{
    using namespace game;

    // TODO: hide wait message

    switch (message->resultCode) {
    case SLNet::L2RC_SUCCESS: {
        CMenuPhase* menuPhase = m_menu->menuBaseData->menuPhase;
        CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::Protocol2CustomLobby);
        break;
    }

    case SLNet::L2RC_Client_Login_HANDLE_NOT_IN_USE_OR_BAD_SECRET_KEY:
        // TODO: textIds, add error caption to the message (see CMenuCustomLobby::showError)
        showMessageBox("Wrong account name or password");
        break;

    case SLNet::L2RC_Client_Login_BANNED:
        // TODO: textIds, add error caption to the message (see CMenuCustomLobby::showError)
        showMessageBox("Banned from server");
        break;

    default: {
        // TODO: textIds, add error caption to the message (see CMenuCustomLobby::showError)
        SLNet::RakString str;
        message->DebugMsg(str);
        showMessageBox(str.C_String());
        break;
    }
    }
}

} // namespace hooks
