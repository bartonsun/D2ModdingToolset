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

#include "netcustomplayerclient.h"
#include "log.h"
#include "mempool.h"
#include "mqnetreception.h"
#include "mqnetsystem.h"
#include "netcustomplayer.h"
#include "netcustomservice.h"
#include "netcustomsession.h"
#include "netmessages.h"
#include "netmsg.h"
#include "settings.h"
#include "utils.h"
#include <fmt/format.h>

namespace hooks {

CNetCustomPlayerClient* CNetCustomPlayerClient::create(CNetCustomSession* session,
                                                       game::IMqNetSystem* system,
                                                       game::IMqNetReception* reception,
                                                       const char* name,
                                                       const SLNet::RakNetGUID& serverGuid)
{
    logDebug("lobby.log", "Creating CNetCustomPlayerClient");

    auto client = (CNetCustomPlayerClient*)game::Memory::get().allocate(
        sizeof(CNetCustomPlayerClient));
    new (client) CNetCustomPlayerClient(session, system, reception, name, serverGuid);
    return client;
}

CNetCustomPlayerClient::CNetCustomPlayerClient(CNetCustomSession* session,
                                               game::IMqNetSystem* system,
                                               game::IMqNetReception* reception,
                                               const char* name,
                                               const SLNet::RakNetGUID& serverGuid)
    : CNetCustomPlayer{session, system, reception, name,
                       getClientId(session->getService()->getPeerGuid())}
    , m_callbacks(this)
    , m_serverGuid{serverGuid}
{
    static game::IMqNetPlayerClientVftable vftable = {
        (game::IMqNetPlayerClientVftable::Destructor)destructor,
        (game::IMqNetPlayerClientVftable::GetName)(GetName)getName,
        (game::IMqNetPlayerClientVftable::GetNetId)getNetId,
        (game::IMqNetPlayerClientVftable::GetSession)(GetSession)getSession,
        (game::IMqNetPlayerClientVftable::GetMessageCount)getMessageCount,
        (game::IMqNetPlayerClientVftable::SendNetMessage)(SendNetMessage)sendMessage,
        (game::IMqNetPlayerClientVftable::ReceiveMessage)receiveMessage,
        (game::IMqNetPlayerClientVftable::SetNetSystem)setNetSystem,
        (game::IMqNetPlayerClientVftable::Method8)method8,
        (game::IMqNetPlayerClientVftable::SetName)(SetName)setName,
        (game::IMqNetPlayerClientVftable::IsHost)isHost,
    };

    this->vftable = &vftable;
    getService()->addPeerCallbacks(&m_callbacks);
}

CNetCustomPlayerClient ::~CNetCustomPlayerClient()
{
    logDebug("lobby.log", "Destroying CNetCustomPlayerClient");
    getService()->removePeerCallbacks(&m_callbacks);
}

void __fastcall CNetCustomPlayerClient::destructor(CNetCustomPlayerClient* thisptr,
                                                   int /*%edx*/,
                                                   char flags)
{
    logDebug("lobby.log", "CNetCustomPlayerClient d-tor");

    thisptr->~CNetCustomPlayerClient();

    if (flags & 1) {
        logDebug("lobby.log", "CNetCustomPlayerClient d-tor frees memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

bool __fastcall CNetCustomPlayerClient::sendMessage(CNetCustomPlayerClient* thisptr,
                                                    int /*%edx*/,
                                                    int idTo,
                                                    const game::NetMessageHeader* message)
{
    if (idTo != game::serverNetPlayerId) {
        // Only send messages to server
        logDebug("lobby.log", "CNetCustomPlayerClient should send messages only to server ???");
        return false;
    }

    return thisptr->sendMessage(message, thisptr->m_serverGuid);
}

bool __fastcall CNetCustomPlayerClient::setName(CNetCustomPlayerClient* thisptr,
                                                int /*%edx*/,
                                                const char* name)
{
    logDebug("lobby.log", "CNetCustomPlayerClient setName");
    thisptr->setName(name);
    return true;
}

bool __fastcall CNetCustomPlayerClient::isHost(CNetCustomPlayerClient* thisptr, int /*%edx*/)
{
    bool isHost = thisptr->getSession()->isHost();
    logDebug("lobby.log", fmt::format("CNetCustomPlayerClient isHost {:d}", isHost));
    return isHost;
}

void CNetCustomPlayerClient::Callbacks::onPacketReceived(DefaultMessageIDTypes type,
                                                         SLNet::RakPeerInterface* peer,
                                                         const SLNet::Packet* packet)
{
    switch (type) {
    case ID_REMOTE_DISCONNECTION_NOTIFICATION:
        logDebug("playerClient.log", "Client disconnected");
        break;
    case ID_REMOTE_CONNECTION_LOST:
        logDebug("playerClient.log", "Client lost connection");
        break;
    case ID_REMOTE_NEW_INCOMING_CONNECTION:
        logDebug("playerClient.log", "Client connected");
        break;
    case ID_CONNECTION_REQUEST_ACCEPTED: {
        logDebug("playerClient.log", "Connection request to the server was accepted");
        auto system = m_player->getSystem();
        if (system) {
            system->vftable->onPlayerConnected(system, (int)getClientId(packet->guid));
        }
        break;
    }
    case ID_NEW_INCOMING_CONNECTION:
        logDebug("playerClient.log", "Incoming connection");
        break;
    case ID_NO_FREE_INCOMING_CONNECTIONS:
        logDebug("playerClient.log", "Server is full");
        break;
    case ID_DISCONNECTION_NOTIFICATION: {
        logDebug("playerClient.log", "Server was shut down");
        auto system = m_player->getSystem();
        if (system) {
            system->vftable->onPlayerDisconnected(system, 1);
        }
        break;
    }
    case ID_CONNECTION_LOST: {
        logDebug("playerClient.log", "Connection with server is lost");
        auto system = m_player->getSystem();
        if (system) {
            system->vftable->onPlayerDisconnected(system, 1);
        }
        break;
    }
    case ID_GAME_MESSAGE: {
        SLNet::RakNetGUID sender(
            *reinterpret_cast<uint64_t*>(packet->data + sizeof(SLNet::MessageID)));
        auto message = reinterpret_cast<const game::NetMessageHeader*>(
            packet->data + sizeof(SLNet::MessageID) + sizeof(uint64_t));
        if (sender != m_player->m_serverGuid) {
            logDebug(
                "lobby.log",
                fmt::format("CNetCustomPlayerClient received message from {:x}, its not a server!",
                            getClientId(packet->guid)));
            break;
        }

        m_player->addMessage(message, game::serverNetPlayerId);
        break;
    }
    case ID_GAME_MESSAGE_TO_HOST_CLIENT: {
        auto message = reinterpret_cast<game::NetMessageHeader*>(packet->data);
        message->messageType = game::netMessageNormalType; // TODO: any better way to do this?
        m_player->addMessage(message, game::serverNetPlayerId);
        break;
    }
    default:
        logDebug("playerClient.log", fmt::format("Packet type {:d}", static_cast<int>(type)));
        break;
    }
}

} // namespace hooks
