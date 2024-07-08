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
    logDebug("lobby.log", "Creating CNetCustomPlayerServer");
    auto server = (CNetCustomPlayerServer*)game::Memory::get().allocate(
        sizeof(CNetCustomPlayerServer));
    new (server) CNetCustomPlayerServer(session, system, reception);
    return server;
}

CNetCustomPlayerServer::CNetCustomPlayerServer(CNetCustomSession* session,
                                               game::IMqNetSystem* system,
                                               game::IMqNetReception* reception)
    : CNetCustomPlayer(session, system, reception, "SERVER", game::serverNetPlayerId)
    , m_peerCallback(this)
    , m_roomsCallback(this)
{
    static game::IMqNetPlayerServerVftable vftable = {
        (game::IMqNetPlayerServerVftable::Destructor)destructor,
        (game::IMqNetPlayerServerVftable::GetName)(GetName)getName,
        (game::IMqNetPlayerServerVftable::GetNetId)getNetId,
        (game::IMqNetPlayerServerVftable::GetSession)(GetSession)getSession,
        (game::IMqNetPlayerServerVftable::GetMessageCount)getMessageCount,
        (game::IMqNetPlayerServerVftable::SendNetMessage)(SendNetMessage)sendMessage,
        (game::IMqNetPlayerServerVftable::ReceiveMessage)receiveMessage,
        (game::IMqNetPlayerServerVftable::SetNetSystem)setNetSystem,
        (game::IMqNetPlayerServerVftable::Method8)method8,
        (game::IMqNetPlayerServerVftable::DestroyPlayer)destroyPlayer,
        (game::IMqNetPlayerServerVftable::SetMaxPlayers)setMaxPlayers,
        (game::IMqNetPlayerServerVftable::SetAllowJoin)setAllowJoin,
    };

    this->vftable = &vftable;
    getService()->addPeerCallback(&m_peerCallback);
    getService()->addRoomsCallback(&m_roomsCallback);
}

CNetCustomPlayerServer::~CNetCustomPlayerServer()
{
    logDebug("lobby.log", "Destroying CNetCustomPlayerServer");
    getService()->removePeerCallback(&m_peerCallback);
    getService()->removeRoomsCallback(&m_roomsCallback);
}

bool CNetCustomPlayerServer::addClient(const SLNet::RakNetGUID& guid, const SLNet::RakString& name)
{
    {
        std::lock_guard lock(m_clientsMutex);
        auto result = m_clients.insert({guid, name});
        if (!result.second) {
            logDebug(
                "lobby.log",
                fmt::format(
                    "CNetCustomPlayerServer::addClient failed because the guid already exists, guid = {:d}, name = {:s}",
                    guid.ToUint32(guid), name.C_String()));
            return false;
        }
    }

    auto system = getSystem();
    system->vftable->onPlayerConnected(system, (int)getClientId(guid));
    return true;
}

bool CNetCustomPlayerServer::removeClient(const SLNet::RakNetGUID& guid)
{
    {
        std::lock_guard lock(m_clientsMutex);
        size_t result = m_clients.erase(guid);
        if (!result) {
            logDebug(
                "lobby.log",
                fmt::format(
                    "CNetCustomPlayerServer::removeClient failed because the guid does not exist, guid = {:d}",
                    guid.ToUint32(guid)));
            return false;
        }
    }

    auto system = getSystem();
    system->vftable->onPlayerDisconnected(system, (int)getClientId(guid));
    return true;
}

bool CNetCustomPlayerServer::removeClient(const SLNet::RakString& name)
{
    SLNet::RakNetGUID guid = SLNet::UNASSIGNED_RAKNET_GUID;
    {
        std::lock_guard lock(m_clientsMutex);
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it->second == name) {
                guid = it->first;
                m_clients.erase(guid);
                break;
            }
        }
    }

    if (guid == SLNet::UNASSIGNED_RAKNET_GUID) {
        logDebug(
            "lobby.log",
            fmt::format(
                "CNetCustomPlayerServer::removeClient failed because the name does not exist, name = {:s}",
                name.C_String()));
        return false;
    }

    auto system = getSystem();
    system->vftable->onPlayerDisconnected(system, (int)getClientId(guid));
    return true;
}

void __fastcall CNetCustomPlayerServer::destructor(CNetCustomPlayerServer* thisptr,
                                                   int /*%edx*/,
                                                   char flags)
{
    logDebug("lobby.log", "CNetCustomPlayerServer d-tor");

    thisptr->~CNetCustomPlayerServer();

    if (flags & 1) {
        logDebug("lobby.log", "CNetCustomPlayerServer d-tor frees memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

bool __fastcall CNetCustomPlayerServer::sendMessage(CNetCustomPlayerServer* thisptr,
                                                    int /*%edx*/,
                                                    int idTo,
                                                    const game::NetMessageHeader* message)
{
    std::set<SLNet::RakNetGUID> clients;
    {
        std::lock_guard lock(thisptr->m_clientsMutex);
        for (const auto& client : thisptr->m_clients) {
            clients.insert(client.first);
        }
    }

    if (idTo == game::broadcastNetPlayerId) {
        logDebug("lobby.log", "CNetCustomPlayerServer sendMessage broadcast");
        return thisptr->sendMessage(message, std::move(clients));
    }

    auto it = std::find_if(clients.begin(), clients.end(), [idTo](const auto& guid) {
        return static_cast<int>(getClientId(guid)) == idTo;
    });
    if (it == clients.end()) {
        logDebug(
            "lobby.log",
            fmt::format("CNetCustomPlayerServer could not send message. No client with id 0x{:x}",
                        uint32_t(idTo)));
        return false;
    }

    return thisptr->sendMessage(message, *it);
}

bool __fastcall CNetCustomPlayerServer::destroyPlayer(CNetCustomPlayerServer* thisptr,
                                                      int /*%edx*/,
                                                      int playerId)
{
    logDebug("lobby.log", "CNetCustomPlayerServer destroyPlayer");
    return false;
}

bool __fastcall CNetCustomPlayerServer::setMaxPlayers(CNetCustomPlayerServer* thisptr,
                                                      int /*%edx*/,
                                                      int maxPlayers)
{
    logDebug("lobby.log", fmt::format("CNetCustomPlayerServer setMaxPlayers {:d}", maxPlayers));
    return thisptr->getSession()->setMaxPlayers(maxPlayers);
}

bool __fastcall CNetCustomPlayerServer::setAllowJoin(CNetCustomPlayerServer* thisptr,
                                                     int /*%edx*/,
                                                     bool allowJoin)
{
    // Ignore this since its only called during server creation and eventually being allowed
    logDebug("lobby.log", fmt::format("CNetCustomPlayerServer setAllowJoin {:d}", (int)allowJoin));
    return true;
}

void CNetCustomPlayerServer::PeerCallback::onPacketReceived(DefaultMessageIDTypes type,
                                                            SLNet::RakPeerInterface* peer,
                                                            const SLNet::Packet* packet)
{
    switch (type) {
    case ID_GAME_MESSAGE: {
        SLNet::RakNetGUID sender;
        auto message = getMessageAndSender(packet, &sender);
        m_player->addMessage(message, getClientId(sender));
        break;
    }

    case ID_GAME_MESSAGE_TO_HOST_SERVER: {
        auto message = reinterpret_cast<game::NetMessageHeader*>(packet->data);
        message->messageType = game::netMessageNormalType; // TODO: any better way to do this?
        m_player->addMessage(message, getClientId(packet->guid));
        break;
    }

    case ID_DISCONNECTION_NOTIFICATION: {
        logDebug("lobby.log", "PlayerServer: Server was shut down");
        m_player->removeClient(packet->guid);
        break;
    }

    case ID_CONNECTION_LOST: {
        logDebug("lobby.log", "PlayerServer: Connection with server is lost");
        m_player->removeClient(packet->guid);
        break;
    }

    default:
        logDebug("lobby.log",
                 fmt::format("PlayerServer: Packet type {:d}", static_cast<int>(type)));
        break;
    }
}

void CNetCustomPlayerServer::RoomsCallback::RoomMemberLeftRoom_Callback(
    const SLNet::SystemAddress& /*senderAddress is the lobby*/,
    SLNet::RoomMemberLeftRoom_Notification* notification)
{
    // TODO: make sure that the notification only arrives for our room, otherwise check roomId
    logDebug("lobby.log", fmt::format("CNetCustomPlayerServer RoomMemberLeftRoom {:s}",
                                      notification->roomMember.C_String()));
    m_player->removeClient(notification->roomMember);
}

void CNetCustomPlayerServer::RoomsCallback::RoomMemberJoinedRoom_Callback(
    const SLNet::SystemAddress& /*senderAddress is the lobby*/,
    SLNet::RoomMemberJoinedRoom_Notification* notification)
{
    // TODO: make sure that the notification only arrives for our room, otherwise check roomId
    const auto result = notification->joinedRoomResult;
    logDebug("lobby.log", fmt::format("CNetCustomPlayerServer RoomMemberJoinedRoom {:s}",
                                      result->joiningMemberName.C_String()));
    m_player->addClient(result->joiningMemberGuid, result->joiningMemberName);
}

} // namespace hooks
