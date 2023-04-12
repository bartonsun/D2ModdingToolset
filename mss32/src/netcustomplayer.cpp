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

#include "netcustomplayer.h"
#include "d2string.h"
#include "log.h"
#include "mempool.h"
#include "mqnetplayer.h"
#include "mqnetreception.h"
#include "mqnetsystem.h"
#include "netcustomservice.h"
#include "netcustomsession.h"
#include "netmsg.h"
#include <fmt/format.h>
#include <mutex>
#include <slikenet/types.h>

namespace hooks {

CNetCustomPlayer::CNetCustomPlayer(CNetCustomSession* session,
                                   game::IMqNetSystem* system,
                                   game::IMqNetReception* reception,
                                   const char* name,
                                   std::uint32_t id)
    : m_name{name}
    , m_session{session}
    , m_system{system}
    , m_reception{reception}
    , m_id{id}
{
    static game::IMqNetPlayerVftable vftable = {
        (game::IMqNetPlayerVftable::Destructor)destructor,
        (game::IMqNetPlayerVftable::GetName)(GetName)getName,
        (game::IMqNetPlayerVftable::GetNetId)getNetId,
        (game::IMqNetPlayerVftable::GetSession)(GetSession)getSession,
        (game::IMqNetPlayerVftable::GetMessageCount)getMessageCount,
        (game::IMqNetPlayerVftable::SendNetMessage)(SendNetMessage)sendMessage,
        (game::IMqNetPlayerVftable::ReceiveMessage)receiveMessage,
        (game::IMqNetPlayerVftable::SetNetSystem)setNetSystem,
        (game::IMqNetPlayerVftable::Method8)method8,
    };

    this->vftable = &vftable;
}

CNetCustomPlayer::~CNetCustomPlayer()
{
    if (m_system) {
        logDebug("lobby.log", "CNetCustomPlayer delete netSystem");
        m_system->vftable->destructor(m_system, 1);
    }

    if (m_reception) {
        logDebug("lobby.log", "CNetCustomPlayer delete netReception");
        m_reception->vftable->destructor(m_reception, 1);
    }
}

uint32_t CNetCustomPlayer::getClientId(const SLNet::RakNetGUID& guid)
{
    return guid.ToUint32(guid);
}

CNetCustomService* CNetCustomPlayer::getService() const
{
    return m_session->getService();
}

CNetCustomSession* CNetCustomPlayer::getSession() const
{
    return m_session;
}

game::IMqNetSystem* CNetCustomPlayer::getSystem() const
{
    return m_system;
}

game::IMqNetReception* CNetCustomPlayer::getReception() const
{
    return m_reception;
}

void CNetCustomPlayer::setName(const char* value)
{
    m_name = value;
}

void CNetCustomPlayer::addMessage(const game::NetMessageHeader* message, std::uint32_t idFrom)
{
    logDebug("lobby.log",
             fmt::format("Game message '{:s}' from {:x}", message->messageClassName, idFrom));

    auto msg = std::make_unique<unsigned char[]>(message->length);
    std::memcpy(msg.get(), message, message->length);
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_messages.push(IdMessagePair{std::uint32_t{idFrom}, std::move(msg)});
    }

    m_reception->vftable->notify(m_reception);
}

bool CNetCustomPlayer::sendMessage(const game::NetMessageHeader* message,
                                   const SLNet::RakNetGUID& to) const
{
    logDebug("lobby.log", fmt::format("CNetCustomService: sendMessage '{:s}' to 0x{:x}",
                                      message->messageClassName, getClientId(to)));

    auto service = getService();
    if (to == service->getPeerGuid()) {
        return sendHostMessage(message);
    }

    SLNet::BitStream stream;
    stream.Write(static_cast<SLNet::MessageID>(ID_GAME_MESSAGE));
    stream.Write(static_cast<uint32_t>(1)); // Recipient count
    stream.Write(to);
    stream.Write((const char*)message, message->length);
    return service->send(stream, service->getLobbyGuid());
}

bool CNetCustomPlayer::sendMessage(const game::NetMessageHeader* message,
                                   std::set<SLNet::RakNetGUID> to) const
{
    logDebug("lobby.log", fmt::format("CNetCustomService: sendMessage '{:s}' to {:d} recipient(s)",
                                      message->messageClassName, to.size()));

    auto service = getService();
    auto host = to.find(service->getPeerGuid());
    if (host != to.end()) {
        if (!sendHostMessage(message)) {
            return false;
        }
        to.erase(host);
    }

    if (to.size() == 0) {
        return true;
    }

    SLNet::BitStream stream;
    stream.Write(static_cast<SLNet::MessageID>(ID_GAME_MESSAGE));
    stream.Write(static_cast<uint32_t>(to.size()));
    for (const auto& recipient : to) {
        stream.Write(recipient);
    }
    stream.Write((const char*)message, message->length);
    return service->send(stream, service->getLobbyGuid());
}

bool CNetCustomPlayer::sendHostMessage(const game::NetMessageHeader* message) const
{
    auto msg = const_cast<game::NetMessageHeader*>(message);
    auto originalType = msg->messageType;
    msg->messageType = m_id == game::serverNetPlayerId ? ID_GAME_MESSAGE_TO_HOST_CLIENT
                                                       : ID_GAME_MESSAGE_TO_HOST_SERVER;

    auto service = getService();
    SLNet::BitStream stream((unsigned char*)message, message->length, false);
    bool result = service->send(stream, service->getPeerGuid());
    msg->messageType = originalType;
    return result;
}

void __fastcall CNetCustomPlayer::destructor(CNetCustomPlayer* thisptr, int /*%edx*/, char flags)
{
    logDebug("lobby.log", "CNetCustomPlayer d-tor called");

    thisptr->~CNetCustomPlayer();

    if (flags & 1) {
        logDebug("lobby.log", "CNetCustomPlayer free memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

game::String* __fastcall CNetCustomPlayer::getName(CNetCustomPlayer* thisptr,
                                                   int /*%edx*/,
                                                   game::String* string)
{
    logDebug("lobby.log", fmt::format("CNetCustomPlayer {:s} getName", thisptr->m_name));
    game::StringApi::get().initFromString(string, thisptr->m_name.c_str());
    return string;
}

int __fastcall CNetCustomPlayer::getNetId(CNetCustomPlayer* thisptr, int /*%edx*/)
{
    logDebug("lobby.log",
             fmt::format("CNetCustomPlayer {:s} getNetId 0x{:x}", thisptr->m_name, thisptr->m_id));
    return static_cast<int>(thisptr->m_id);
}

game::IMqNetSession* __fastcall CNetCustomPlayer::getSession(CNetCustomPlayer* thisptr,
                                                             int /*%edx*/)
{
    logDebug("lobby.log", fmt::format("CNetCustomPlayer {:s} getSession", thisptr->m_name));
    return thisptr->m_session;
}

int __fastcall CNetCustomPlayer::getMessageCount(CNetCustomPlayer* thisptr, int /*%edx*/)
{
    logDebug("lobby.log", fmt::format("CNetCustomPlayer {:s} getMessageCount", thisptr->m_name));

    std::lock_guard<std::mutex> messageGuard(thisptr->m_messagesMutex);

    return static_cast<int>(thisptr->m_messages.size());
}

bool __fastcall CNetCustomPlayer::sendMessage(CNetCustomPlayer* thisptr,
                                              int /*%edx*/,
                                              int idTo,
                                              const game::NetMessageHeader* message)
{
    logDebug("lobby.log", fmt::format("CNetCustomPlayer {:s} sendMessage '{:s}' to {:d}",
                                      thisptr->m_name, message->messageClassName, idTo));
    return true;
}

game::ReceiveMessageResult __fastcall CNetCustomPlayer::receiveMessage(
    CNetCustomPlayer* thisptr,
    int /*%edx*/,
    int* idFrom,
    game::NetMessageHeader* buffer)
{
    if (!idFrom || !buffer) {
        // This should never happen
        return game::ReceiveMessageResult::Failure;
    }

    logDebug("lobby.log", fmt::format("CNetCustomPlayer '{:s}' receiveMessage", thisptr->m_name));

    std::lock_guard<std::mutex> messageGuard(thisptr->m_messagesMutex);

    if (thisptr->m_messages.empty()) {
        return game::ReceiveMessageResult::NoMessages;
    }

    const auto& pair = thisptr->m_messages.front();
    auto message = reinterpret_cast<const game::NetMessageHeader*>(pair.second.get());

    if (message->messageType != game::netMessageNormalType) {
        logDebug("lobby.log",
                 fmt::format("CNetCustomPlayer '{:s}' received message with invalid type",
                             thisptr->m_name));
        return game::ReceiveMessageResult::Failure;
    }

    if (message->length >= game::netMessageMaxLength) {
        logDebug("lobby.log",
                 fmt::format("CNetCustomPlayer '{:s}' received message with invalid length",
                             thisptr->m_name));
        return game::ReceiveMessageResult::Failure;
    }

    logDebug("lobby.log",
             fmt::format("CNetCustomPlayer '{:s}' receiveMessage '{:s}' length {:d} from 0x{:x}",
                         thisptr->m_name, message->messageClassName, message->length, pair.first));

    *idFrom = static_cast<int>(pair.first);
    std::memcpy(buffer, message, message->length);

    thisptr->m_messages.pop();
    return game::ReceiveMessageResult::Success;
}

void __fastcall CNetCustomPlayer::setNetSystem(CNetCustomPlayer* thisptr,
                                               int /*%edx*/,
                                               game::IMqNetSystem* netSystem)
{
    logDebug("lobby.log", fmt::format("CNetCustomPlayer {:s} setNetSystem", thisptr->m_name));
    if (thisptr->m_system != netSystem) {
        if (thisptr->m_system) {
            thisptr->m_system->vftable->destructor(thisptr->m_system, 1);
        }
        thisptr->m_system = netSystem;
    }
}

int __fastcall CNetCustomPlayer::method8(CNetCustomPlayer* thisptr, int /*%edx*/, int a2)
{
    logDebug("lobby.log", fmt::format("CNetCustomPlayer {:s} method8", thisptr->m_name));
    return a2;
}

} // namespace hooks
