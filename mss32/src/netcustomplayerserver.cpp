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

#include "netcustomplayerserver.h"
#include "log.h"
#include "mempool.h"
#include "mqnetreception.h"
#include "mqnetsystem.h"
#include "netcustomplayer.h"
#include "netcustomsession.h"
#include "netmsg.h"
#include "utils.h"
#include <BitStream.h>
#include <MessageIdentifiers.h>
#include <algorithm>
#include <fmt/format.h>
#include <mutex>

namespace hooks {

CNetCustomPlayerServer* CNetCustomPlayerServer::create(CNetCustomSession* session,
                                                       game::IMqNetSystem* system,
                                                       game::IMqNetReception* reception)
{
    playerLog("Creating CNetCustomPlayerServer");
    auto server = (CNetCustomPlayerServer*)game::Memory::get().allocate(
        sizeof(CNetCustomPlayerServer));
    new (server) CNetCustomPlayerServer(session, system, reception);
    return server;
}

CNetCustomPlayerServer::CNetCustomPlayerServer(CNetCustomSession* session,
                                               game::IMqNetSystem* system,
                                               game::IMqNetReception* reception)
    : CNetCustomPlayer{session, system, reception, "SERVER", game::serverNetPlayerId}
    , m_callbacks{this}
{
    static game::IMqNetPlayerServerVftable vftable = {
        (game::IMqNetPlayerServerVftable::Destructor)destructor,
        (game::IMqNetPlayerServerVftable::GetName)(GetName)getName,
        (game::IMqNetPlayerServerVftable::GetNetId)getNetId,
        (game::IMqNetPlayerServerVftable::GetSession)(GetSession)getSession,
        (game::IMqNetPlayerServerVftable::GetMessageCount)getMessageCount,
        (game::IMqNetPlayerServerVftable::SendNetMessage)sendMessage,
        (game::IMqNetPlayerServerVftable::ReceiveMessage)receiveMessage,
        (game::IMqNetPlayerServerVftable::SetNetSystem)setNetSystem,
        (game::IMqNetPlayerServerVftable::Method8)method8,
        (game::IMqNetPlayerServerVftable::DestroyPlayer)destroyPlayer,
        (game::IMqNetPlayerServerVftable::SetMaxPlayers)setMaxPlayers,
        (game::IMqNetPlayerServerVftable::SetAllowJoin)setAllowJoin,
    };

    this->vftable = &vftable;
    session->getService()->addPeerCallbacks(&m_callbacks);
}

bool CNetCustomPlayerServer::notifyHostClientConnected()
{
    auto system = getSystem();
    if (!system) {
        playerLog("PlayerServer: no netSystem in notifyHostClientConnected()");
        return false;
    }

    if (m_connectedIds.empty()) {
        playerLog("PlayerServer: host client is not connected");
        return false;
    }

    const std::uint32_t hostClientNetId{SLNet::RakNetGUID::ToUint32(m_connectedIds[0])};
    playerLog(fmt::format("PlayerServer: onPlayerConnected 0x{:x}", hostClientNetId));

    system->vftable->onPlayerConnected(system, (int)hostClientNetId);
    return true;
}

void __fastcall CNetCustomPlayerServer::destructor(CNetCustomPlayerServer* thisptr,
                                                   int /*%edx*/,
                                                   char flags)
{
    playerLog("CNetCustomPlayerServer d-tor");

    thisptr->~CNetCustomPlayerServer();

    if (flags & 1) {
        playerLog("CNetCustomPlayerServer d-tor frees memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

int __fastcall CNetCustomPlayerServer::getMessageCount(CNetCustomPlayerServer* thisptr,
                                                       int /*%edx*/)
{
    playerLog("CNetCustomPlayerServer getMessageCount");

    std::lock_guard<std::mutex> messageGuard(thisptr->m_messagesMutex);

    return static_cast<int>(thisptr->m_messages.size());
}

bool __fastcall CNetCustomPlayerServer::sendMessage(CNetCustomPlayerServer* thisptr,
                                                    int /*%edx*/,
                                                    int idTo,
                                                    const game::NetMessageHeader* message)
{
    const auto& connectedIds = thisptr->m_connectedIds;
    if (idTo == game::broadcastNetPlayerId) {
        playerLog("CNetCustomPlayerServer sendMessage broadcast");
        return thisptr->getService()->sendMessage(message, connectedIds);
    }

    auto it = std::find_if(connectedIds.begin(), connectedIds.end(), [idTo](const auto& guid) {
        return static_cast<int>(SLNet::RakNetGUID::ToUint32(guid)) == idTo;
    });

    if (it == connectedIds.end()) {
        playerLog(
            fmt::format("CNetCustomPlayerServer could not send message. No client with id 0x{:x}",
                        uint32_t(idTo)));
        return false;
    }

    return thisptr->getService()->sendMessage(message, *it);
}

game::ReceiveMessageResult __fastcall CNetCustomPlayerServer::receiveMessage(
    CNetCustomPlayerServer* thisptr,
    int /*%edx*/,
    int* idFrom,
    game::NetMessageHeader* buffer)
{
    if (!idFrom || !buffer) {
        // This should never happen
        return game::ReceiveMessageResult::Failure;
    }

    playerLog("CNetCustomPlayerServer receiveMessage");

    std::lock_guard<std::mutex> messageGuard(thisptr->m_messagesMutex);

    if (thisptr->m_messages.empty()) {
        return game::ReceiveMessageResult::NoMessages;
    }

    const auto& pair = thisptr->m_messages.front();
    auto message = reinterpret_cast<const game::NetMessageHeader*>(pair.second.get());

    if (message->messageType != game::netMessageNormalType) {
        playerLog("CNetCustomPlayerServer received message with invalid type");
        return game::ReceiveMessageResult::Failure;
    }

    if (message->length >= game::netMessageMaxLength) {
        playerLog("CNetCustomPlayerServer received message with invalid length");
        return game::ReceiveMessageResult::Failure;
    }

    playerLog(fmt::format("CNetCustomPlayerServer receiveMessage '{:s}' length {:d} from 0x{:x}",
                          message->messageClassName, message->length, pair.first));

    *idFrom = static_cast<int>(pair.first);
    std::memcpy(buffer, message, message->length);

    thisptr->m_messages.pop_front();
    return game::ReceiveMessageResult::Success;
}

bool __fastcall CNetCustomPlayerServer::destroyPlayer(CNetCustomPlayerServer* thisptr,
                                                      int /*%edx*/,
                                                      int playerId)
{
    playerLog("CNetCustomPlayerServer destroyPlayer");
    return false;
}

bool __fastcall CNetCustomPlayerServer::setMaxPlayers(CNetCustomPlayerServer* thisptr,
                                                      int /*%edx*/,
                                                      int maxPlayers)
{
    playerLog(fmt::format("CNetCustomPlayerServer setMaxPlayers {:d}", maxPlayers));
    return thisptr->getSession()->setMaxPlayers(maxPlayers);
}

bool __fastcall CNetCustomPlayerServer::setAllowJoin(CNetCustomPlayerServer* thisptr,
                                                     int /*%edx*/,
                                                     bool allowJoin)
{
    // Ignore this since its only called during server creation and eventually being allowed
    playerLog(fmt::format("CNetCustomPlayerServer setAllowJoin {:d}", (int)allowJoin));
    return true;
}

void CNetCustomPlayerServer::Callbacks::onPacketReceived(DefaultMessageIDTypes type,
                                                         SLNet::RakPeerInterface* peer,
                                                         const SLNet::Packet* packet)
{
    auto netSystem{m_player->getSystem()};

    switch (type) {
    case ID_REMOTE_DISCONNECTION_NOTIFICATION: {
        logDebug("lobby.log", "PlayerServer: Client disconnected");

        if (netSystem) {
            auto guid = peer->GetGuidFromSystemAddress(packet->systemAddress);
            auto guidInt = SLNet::RakNetGUID::ToUint32(guid);

            netSystem->vftable->onPlayerDisconnected(netSystem, (int)guidInt);
        }

        break;
    }
    case ID_REMOTE_CONNECTION_LOST: {
        logDebug("lobby.log", "PlayerServer: Client lost connection");

        auto guid = peer->GetGuidFromSystemAddress(packet->systemAddress);

        auto& ids = m_player->m_connectedIds;
        ids.erase(std::remove(ids.begin(), ids.end(), guid), ids.end());

        if (netSystem) {
            auto guidInt = SLNet::RakNetGUID::ToUint32(guid);
            netSystem->vftable->onPlayerDisconnected(netSystem, (int)guidInt);
        }

        break;
    }
    case ID_REMOTE_NEW_INCOMING_CONNECTION:
        logDebug("lobby.log", "PlayerServer: Client connected");
        break;
    case ID_CONNECTION_REQUEST_ACCEPTED:
        // This should never happen on server ?
        logDebug("lobby.log", "PlayerServer: Connection request to the server was accepted");
        break;
    case ID_NEW_INCOMING_CONNECTION: {
        auto guid = peer->GetGuidFromSystemAddress(packet->systemAddress);
        auto guidInt = SLNet::RakNetGUID::ToUint32(guid);

        logDebug("lobby.log", fmt::format("PlayerServer: Incoming connection, id 0x{:x}", guidInt));

        m_player->m_connectedIds.push_back(guid);

        if (netSystem) {
            logDebug("lobby.log", "PlayerServer: Call netSystem onPlayerConnected");
            netSystem->vftable->onPlayerConnected(netSystem, (int)guidInt);
        } else {
            logDebug("lobby.log",
                     "PlayerServer: no netSystem is set, skip onPlayerConnected notification");
        }

        break;
    }
    case ID_NO_FREE_INCOMING_CONNECTIONS:
        // This should never happen on server ?
        logDebug("lobby.log", "PlayerServer: Server is full");
        break;
    case ID_DISCONNECTION_NOTIFICATION: {
        logDebug("lobby.log", "PlayerServer: Client has disconnected from server");

        if (netSystem) {
            auto guid = peer->GetGuidFromSystemAddress(packet->systemAddress);
            auto guidInt = SLNet::RakNetGUID::ToUint32(guid);

            netSystem->vftable->onPlayerDisconnected(netSystem, (int)guidInt);
        }

        break;
    }
    case ID_CONNECTION_LOST: {
        logDebug("lobby.log", "PlayerServer: Client has lost connection");

        if (netSystem) {
            auto guid = peer->GetGuidFromSystemAddress(packet->systemAddress);
            auto guidInt = SLNet::RakNetGUID::ToUint32(guid);

            netSystem->vftable->onPlayerDisconnected(netSystem, (int)guidInt);
        }

        break;
    }
    case 0xff: {
        // Game message received
        auto message = reinterpret_cast<const game::NetMessageHeader*>(packet->data);

        auto guid = peer->GetGuidFromSystemAddress(packet->systemAddress);
        auto guidInt = SLNet::RakNetGUID::ToUint32(guid);

        /*logDebug("playerServer.log", fmt::format("Game message '{:s}' from {:x}",
                                                 message->messageClassName, guidInt));*/

        // logDebug("playerServer.log", "Allocate net message");

        auto msg = std::make_unique<unsigned char[]>(message->length);
        // logDebug("playerServer.log", "Copy net message");
        std::memcpy(msg.get(), message, message->length);

        {
            std::lock_guard<std::mutex> messageGuard(m_player->m_messagesMutex);
            m_player->m_messages.push_back(
                CNetCustomPlayerServer::IdMessagePair{std::uint32_t{guidInt}, std::move(msg)});
        }

        auto reception = m_player->getReception();
        if (reception) {
            reception->vftable->notify(reception);
        }

        break;
    }
    default:
        logDebug("lobby.log",
                 fmt::format("PlayerServer: Packet type {:d}", static_cast<int>(packet->data[0])));
        break;
    }
}

} // namespace hooks
