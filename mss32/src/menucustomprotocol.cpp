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
#include "mempool.h"
#include "menuflashwait.h"
#include "menuphase.h"
#include "textids.h"
#include "utils.h"

namespace hooks {

CMenuCustomProtocol::CMenuCustomProtocol(game::CMenuPhase* menuPhase)
    : m_timeoutEvent{}
    , m_menuWait{nullptr}
    , m_callbacks{this}
{
    using namespace game;

    CMenuProtocolApi::get().constructor(this, menuPhase);

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBox = CDialogInterfApi::get().findListBox(dialog, "TLBOX_PROTOCOL");
    if (listBox) {
        // One more entry for our custom protocol
        CListBoxInterfApi::get().setElementsTotal(listBox, listBox->listBoxData->elementsTotal + 1);
    }
}

void CMenuCustomProtocol::waitConnectionAsync()
{
    using namespace game;

    m_menuWait = (CMenuFlashWait*)Memory::get().allocate(sizeof(CMenuFlashWait));
    CMenuFlashWaitApi::get().constructor(m_menuWait);
    showInterface(m_menuWait);

    createTimerEvent(&m_timeoutEvent, this, timeoutHandler,
                     CNetCustomService::connectionWaitTimeout);

    getNetService()->addPeerCallbacks(&m_callbacks);
}

void CMenuCustomProtocol::stopWaitingConnection()
{
    using namespace game;

    if (!m_menuWait) {
        return;
    }

    getNetService()->removePeerCallbacks(&m_callbacks);

    UiEventApi::get().destructor(&m_timeoutEvent);

    hideInterface(m_menuWait);
    m_menuWait->vftable->destructor(m_menuWait, 1);
    m_menuWait = nullptr;
}

void CMenuCustomProtocol::showConnectionError(const char* errorMessage)
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

        m_menu->showConnectionError(message.c_str());
        return;
    }
    case ID_NO_FREE_INCOMING_CONNECTIONS: {
        auto message{getInterfaceText(textIds().lobby.serverIsFull.c_str())};
        if (message.empty()) {
            message = "Lobby server is full";
        }

        m_menu->showConnectionError(message.c_str());
        return;
    }
    case ID_ALREADY_CONNECTED:
        m_menu->showConnectionError("Already connected.\nThis should never happen");
        return;

    // TODO: move integrity request to custom service callbacks, only check results here
    case ID_CONNECTION_REQUEST_ACCEPTED: {
        std::string hash;
        if (!computeHash(globalsFolder(), hash)) {
            auto message{getInterfaceText(textIds().lobby.computeHashFailed.c_str())};
            if (message.empty()) {
                message = "Could not compute hash";
            }

            m_menu->showConnectionError(message.c_str());
            return;
        }

        if (!getNetService()->checkFilesIntegrity(hash.c_str())) {
            auto message{getInterfaceText(textIds().lobby.requestHashCheckFailed.c_str())};
            if (message.empty()) {
                message = "Could not request game integrity check";
            }

            m_menu->showConnectionError(message.c_str());
            return;
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

            m_menu->showConnectionError(message.c_str());
            return;
        }

        // Switch to custom lobby window
        game::CMenuPhaseApi::get().setTransition(m_menu->menuBaseData->menuPhase, 2);
        return;
    }

    default:
        return;
    }
}

void __fastcall CMenuCustomProtocol::timeoutHandler(CMenuCustomProtocol* menu, int /*%edx*/)
{
    auto message{getInterfaceText(textIds().lobby.serverNotResponding.c_str())};
    if (message.empty()) {
        message = "Failed to connect.\nLobby server not responding";
    }

    menu->showConnectionError(message.c_str());
}

} // namespace hooks
