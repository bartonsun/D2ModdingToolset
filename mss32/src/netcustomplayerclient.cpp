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
#include <slikenet/types.h>

namespace hooks {

CNetCustomPlayerClient* CNetCustomPlayerClient::create(CNetCustomSession* session)
{
    logDebug("lobby.log", "Creating CNetCustomPlayerClient");

    // Empty fields will be initialized later
    auto client = (CNetCustomPlayerClient*)game::Memory::get().allocate(
        sizeof(CNetCustomPlayerClient));
    new (client)
        CNetCustomPlayerClient(session, nullptr, nullptr, "", 0, SLNet::UNASSIGNED_RAKNET_GUID, 0);
    return client;
}

CNetCustomPlayerClient::CNetCustomPlayerClient(CNetCustomSession* session,
                                               game::IMqNetSystem* system,
                                               game::IMqNetReception* reception,
                                               const char* name,
                                               std::uint32_t id,
                                               const SLNet::RakNetGUID& serverAddress,
                                               std::uint32_t serverId)
    : CNetCustomPlayer{session, system, reception, name, id}
    , m_callbacks(this)
    , m_serverAddress{serverAddress}
    , m_serverId{serverId}
{
    static game::IMqNetPlayerClientVftable vftable = {
        (game::IMqNetPlayerClientVftable::Destructor)destructor,
        (game::IMqNetPlayerClientVftable::GetName)(GetName)getName,
        (game::IMqNetPlayerClientVftable::GetNetId)getNetId,
        (game::IMqNetPlayerClientVftable::GetSession)(GetSession)getSession,
        (game::IMqNetPlayerClientVftable::GetMessageCount)getMessageCount,
        (game::IMqNetPlayerClientVftable::SendNetMessage)sendMessage,
        (game::IMqNetPlayerClientVftable::ReceiveMessage)receiveMessage,
        (game::IMqNetPlayerClientVftable::SetNetSystem)setNetSystem,
        (game::IMqNetPlayerClientVftable::Method8)method8,
        (game::IMqNetPlayerClientVftable::SetName)(SetName)setName,
        (game::IMqNetPlayerClientVftable::IsHost)isHost,
    };

    this->vftable = &vftable;
}

void CNetCustomPlayerClient::setupPacketCallbacks()
{
    logDebug("lobby.log", "Setup player client packet callbacks");
    getService()->addPeerCallbacks(&m_callbacks);
}

const SLNet::RakNetGUID& CNetCustomPlayerClient::getServerAddress() const
{
    return m_serverAddress;
}

void CNetCustomPlayerClient::setServerAddress(const SLNet::RakNetGUID& value)
{
    m_serverAddress = value;
}

std::uint32_t CNetCustomPlayerClient::getServerId() const
{
    return m_serverId;
}

void CNetCustomPlayerClient::setServerId(std::uint32_t value)
{
    m_serverId = value;
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

    return thisptr->getService()->sendMessage(message, thisptr->m_serverAddress);
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
    auto netSystem{m_player->getSystem()};

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

        if (netSystem) {
            auto guid = peer->GetGuidFromSystemAddress(packet->systemAddress);
            auto guidInt = SLNet::RakNetGUID::ToUint32(guid);

            netSystem->vftable->onPlayerConnected(netSystem, (int)guidInt);
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

        if (netSystem) {
            netSystem->vftable->onPlayerDisconnected(netSystem, 1);
        }

        break;
    }
    case ID_CONNECTION_LOST: {
        logDebug("playerClient.log", "Connection with server is lost");

        if (netSystem) {
            netSystem->vftable->onPlayerDisconnected(netSystem, 1);
        }

        break;
    }
    case 0xff: {
        // Game message received
        auto sender = peer->GetGuidFromSystemAddress(packet->systemAddress);
        auto senderId = sender.ToUint32(sender);
        if (std::uint32_t{senderId} != m_player->m_serverId) {
            logDebug(
                "lobby.log",
                fmt::format("CNetCustomPlayerClient received message from {:x}, its not a server!",
                            senderId));
            break;
        }

        m_player->addMessage(sender, packet);
        break;
    }
    default:
        logDebug("playerClient.log", fmt::format("Packet type {:d}", static_cast<int>(type)));
        break;
    }
}

} // namespace hooks
