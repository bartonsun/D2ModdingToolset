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

CNetCustomPlayerClient::CNetCustomPlayerClient(CNetCustomSession* session,
                                               game::IMqNetSystem* system,
                                               game::IMqNetReception* reception,
                                               const char* name,
                                               const SLNet::RakNetGUID& serverGuid)
    : CNetCustomPlayer{session, system, reception, name,
                       getClientId(session->getService()->getPeerGuid())}
    , m_peerCallback(this)
    , m_roomsCallback(this)
    , m_serverGuid{serverGuid}
{
    logDebug("lobby.log", __FUNCTION__);

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
    auto service = getService();
    service->addPeerCallback(&m_peerCallback);
    service->addRoomsCallback(&m_roomsCallback);
}

CNetCustomPlayerClient ::~CNetCustomPlayerClient()
{
    logDebug("lobby.log", __FUNCTION__);
    auto service = getService();
    service->removeRoomsCallback(&m_roomsCallback);
    service->removePeerCallback(&m_peerCallback);
}

void __fastcall CNetCustomPlayerClient::destructor(CNetCustomPlayerClient* thisptr,
                                                   int /*%edx*/,
                                                   char flags)
{
    thisptr->~CNetCustomPlayerClient();

    if (flags & 1) {
        logDebug("lobby.log", __FUNCTION__ ": freeing memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

bool __fastcall CNetCustomPlayerClient::sendMessage(CNetCustomPlayerClient* thisptr,
                                                    int /*%edx*/,
                                                    int idTo,
                                                    const game::NetMessageHeader* message)
{
    if (idTo != game::serverNetPlayerId) {
        logDebug("lobby.log",
                 __FUNCTION__ ": denying sending message to a player other than the server");
        return false;
    }

    return thisptr->sendMessage(message, thisptr->m_serverGuid);
}

bool __fastcall CNetCustomPlayerClient::setName(CNetCustomPlayerClient* thisptr,
                                                int /*%edx*/,
                                                const char* name)
{
    logDebug("lobby.log", fmt::format(__FUNCTION__ ": name = '{:s}'", name));
    thisptr->setName(name);
    return true;
}

bool __fastcall CNetCustomPlayerClient::isHost(CNetCustomPlayerClient* thisptr, int /*%edx*/)
{
    bool result = thisptr->getSession()->isHost();
    logDebug("lobby.log", fmt::format(__FUNCTION__ ": is host = {:d}", (int)result));
    return result;
}

void CNetCustomPlayerClient::PeerCallback::onPacketReceived(DefaultMessageIDTypes type,
                                                            SLNet::RakPeerInterface* peer,
                                                            const SLNet::Packet* packet)
{
    switch (type) {
    case ID_GAME_MESSAGE: {
        SLNet::RakNetGUID sender;
        auto message = getMessageAndSender(packet, &sender);
        if (sender != m_player->m_serverGuid) {
            logDebug("lobby.log",
                     fmt::format(__FUNCTION__ ": '{:s}' from 0x{:x}, its not a server!",
                                 message->messageClassName, getClientId(packet->guid)));
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

    case ID_DISCONNECTION_NOTIFICATION: {
        logDebug("lobby.log", __FUNCTION__ ": server was shut down");
        auto system = m_player->getSystem();
        if (system) {
            system->vftable->onPlayerDisconnected(system, game::serverNetPlayerId);
        }
        break;
    }

    case ID_CONNECTION_LOST: {
        logDebug("lobby.log", __FUNCTION__ ": connection with server is lost");
        auto system = m_player->getSystem();
        if (system) {
            system->vftable->onPlayerDisconnected(system, game::serverNetPlayerId);
        }
        break;
    }
    }
}

void CNetCustomPlayerClient::RoomsCallback::RoomDestroyedOnModeratorLeft_Callback(
    const SLNet::SystemAddress& senderAddress,
    SLNet::RoomDestroyedOnModeratorLeft_Notification* notification)
{
    // TODO: make sure that the notification only arrives for our room, otherwise check roomId
    logDebug("lobby.log", __FUNCTION__);
    auto system = m_player->getSystem();
    if (system) {
        system->vftable->onPlayerDisconnected(system, game::serverNetPlayerId);
    }
}

} // namespace hooks
