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
#include <BitStream.h>
#include <MessageIdentifiers.h>
#include <fmt/format.h>

namespace hooks {

NetworkPeer::PeerPtr createPeer(CNetCustomSession* session)
{
    using namespace game;

    const std::uint16_t clientPort = CNetCustomPlayer::clientPort
                                     + userSettings().lobby.client.port;
    SLNet::SocketDescriptor descriptor{clientPort, nullptr};

    // Non-host client players have two connections:
    // with lobby server for NAT traversal and with player server
    auto peer{NetworkPeer::PeerPtr(SLNet::RakPeerInterface::GetInstance())};
    const auto result{peer->Startup(session->isHost() ? 1 : 2, &descriptor, 1)};
    if (result != SLNet::StartupResult::RAKNET_STARTED) {
        playerLog("Failed to start peer for CNetCustomPlayerClient");
        return nullptr;
    }

    return peer;
}

bool connectToServer(NetworkPeer::PeerPtr& peer, const SLNet::SystemAddress& serverAddress)
{
    auto serverIp{serverAddress.ToString(false)};
    playerLog(fmt::format("Host client tries to connect to server at '{:s}', port {:d}", serverIp,
                          CNetCustomPlayer::serverPort));

    const auto result{peer->Connect(serverIp, CNetCustomPlayer::serverPort, nullptr, 0)};
    if (result != SLNet::ConnectionAttemptResult::CONNECTION_ATTEMPT_STARTED) {
        playerLog(fmt::format("Failed to start CNetCustomPlayerClient connection. Error: {:d}",
                              (int)result));
        return false;
    }

    return true;
}

CNetCustomPlayerClient* CNetCustomPlayerClient::create(CNetCustomSession* session)
{
    using namespace game;

    playerLog("Creating CNetCustomPlayerClient");

    auto peer = createPeer(session);
    if (!peer) {
        return nullptr;
    }

    auto serverAddress = SLNet::UNASSIGNED_SYSTEM_ADDRESS;
    if (session->isHost()) {
        // Create player server address from our local address
        // since host player client is on the same machine as player server
        serverAddress = lobbyAddressToServerPlayer(
            session->getService()->lobbyPeer.peer->GetMyBoundAddress());
        if (!connectToServer(peer, serverAddress)) {
            return nullptr;
        }
    }

    // Empty fields will be initialized later
    auto client = (CNetCustomPlayerClient*)Memory::get().allocate(sizeof(CNetCustomPlayerClient));
    new (client)
        CNetCustomPlayerClient(session, nullptr, nullptr, "", std::move(peer), 0, serverAddress, 0);
    return client;
}

CNetCustomPlayerClient::CNetCustomPlayerClient(CNetCustomSession* session,
                                               game::IMqNetSystem* system,
                                               game::IMqNetReception* reception,
                                               const char* name,
                                               NetworkPeer::PeerPtr&& peer,
                                               std::uint32_t id,
                                               const SLNet::SystemAddress& serverAddress,
                                               std::uint32_t serverId)
    : CNetCustomPlayer{session, system, reception, name, std::move(peer), id}
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
    playerLog("Setup player client packet callbacks");
    getPeer().addCallback(&m_callbacks);
}

const SLNet::SystemAddress& CNetCustomPlayerClient::getServerAddress() const
{
    return m_serverAddress;
}

void CNetCustomPlayerClient::setServerAddress(const SLNet::SystemAddress& value)
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

SLNet::NatPunchthroughClient& CNetCustomPlayerClient::getNatClient()
{
    return m_natClient;
}

void __fastcall CNetCustomPlayerClient::destructor(CNetCustomPlayerClient* thisptr,
                                                   int /*%edx*/,
                                                   char flags)
{
    playerLog("CNetCustomPlayerClient d-tor");

    thisptr->~CNetCustomPlayerClient();

    if (flags & 1) {
        playerLog("CNetCustomPlayerClient d-tor frees memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

int __fastcall CNetCustomPlayerClient::getMessageCount(CNetCustomPlayerClient* thisptr,
                                                       int /*%edx*/)
{
    playerLog("CNetCustomPlayerClient getMessageCount");

    std::lock_guard<std::mutex> messageGuard(thisptr->m_messagesMutex);

    return static_cast<int>(thisptr->m_messages.size());
}

bool __fastcall CNetCustomPlayerClient::sendMessage(CNetCustomPlayerClient* thisptr,
                                                    int /*%edx*/,
                                                    int idTo,
                                                    const game::NetMessageHeader* message)
{
    auto peer = thisptr->getPeer().peer.get();
    if (!peer) {
        playerLog("CNetCustomPlayerClient could not send message, peer is nullptr");
        return false;
    }

    auto serverGuid = peer->GetGuidFromSystemAddress(thisptr->m_serverAddress);
    auto serverId = SLNet::RakNetGUID::ToUint32(serverGuid);

    playerLog(
        fmt::format("CNetCustomPlayerClient {:s} sendMessage '{:s}' to {:x}, server guid 0x{:x}",
                    thisptr->getName(), message->messageClassName, std::uint32_t(idTo), serverId));

    if (idTo != game::serverNetPlayerId) {
        playerLog("CNetCustomPlayerClient should send messages only to server ???");
        // Only send messages to server
        return false;
    }

    SLNet::BitStream stream((unsigned char*)message, message->length, false);

    if (peer->Send(&stream, PacketPriority::HIGH_PRIORITY, PacketReliability::RELIABLE_ORDERED, 0,
                   thisptr->m_serverAddress, false)
        == 0) {
        playerLog(
            fmt::format("CNetCustomPlayerClient {:s} Send returned bad input", thisptr->getName()));
    }

    return true;
}

game::ReceiveMessageResult __fastcall CNetCustomPlayerClient::receiveMessage(
    CNetCustomPlayerClient* thisptr,
    int /*%edx*/,
    int* idFrom,
    game::NetMessageHeader* buffer)
{
    if (!idFrom || !buffer) {
        // This should never happen
        return game::ReceiveMessageResult::Failure;
    }

    playerLog("CNetCustomPlayerClient receiveMessage");

    std::lock_guard<std::mutex> messageGuard(thisptr->m_messagesMutex);

    if (thisptr->m_messages.empty()) {
        return game::ReceiveMessageResult::NoMessages;
    }

    const auto& pair = thisptr->m_messages.front();
    const auto& id{pair.first};
    if (id != thisptr->m_serverId) {
        playerLog(
            fmt::format("CNetCustomPlayerClient received message from {:x}, its not a server!",
                        id));
        return game::ReceiveMessageResult::NoMessages;
    }

    auto message = reinterpret_cast<const game::NetMessageHeader*>(pair.second.get());

    if (message->messageType != game::netMessageNormalType) {
        playerLog("CNetCustomPlayerClient received message with invalid type");
        return game::ReceiveMessageResult::Failure;
    }

    if (message->length >= game::netMessageMaxLength) {
        playerLog("CNetCustomPlayerClient received message with invalid length");
        return game::ReceiveMessageResult::Failure;
    }

    playerLog(fmt::format("CNetCustomPlayerClient receiveMessage '{:s}' length {:d} from 0x{:x}",
                          message->messageClassName, message->length, pair.first));

    *idFrom = static_cast<int>(id);
    std::memcpy(buffer, message, message->length);

    thisptr->m_messages.pop_front();
    return game::ReceiveMessageResult::Success;
}

bool __fastcall CNetCustomPlayerClient::setName(CNetCustomPlayerClient* thisptr,
                                                int /*%edx*/,
                                                const char* name)
{
    playerLog("CNetCustomPlayerClient setName");
    thisptr->setName(name);
    return true;
}

bool __fastcall CNetCustomPlayerClient::isHost(CNetCustomPlayerClient* thisptr, int /*%edx*/)
{
    bool isHost = thisptr->getSession()->isHost();
    playerLog(fmt::format("CNetCustomPlayerClient isHost {:d}", isHost));
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
        auto message = reinterpret_cast<const game::NetMessageHeader*>(packet->data);

        auto guid = peer->GetGuidFromSystemAddress(packet->systemAddress);
        auto guidInt = SLNet::RakNetGUID::ToUint32(guid);

        logDebug("playerClient.log",
                 fmt::format("Game message '{:s}' from {:x}", message->messageClassName, guidInt));

        auto msg = std::make_unique<unsigned char[]>(message->length);
        std::memcpy(msg.get(), message, message->length);

        {
            std::lock_guard<std::mutex> messageGuard(m_player->m_messagesMutex);
            m_player->m_messages.push_back(
                CNetCustomPlayerClient::IdMessagePair{std::uint32_t{guidInt}, std::move(msg)});
        }

        auto reception = m_player->getReception();
        if (reception) {
            reception->vftable->notify(reception);
        }

        break;
    }
    default:
        logDebug("playerClient.log", fmt::format("Packet type {:d}", static_cast<int>(type)));
        break;
    }
}

} // namespace hooks
