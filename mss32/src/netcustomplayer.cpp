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
#include "netcustomsession.h"
#include "netmsg.h"
#include <fmt/format.h>
#include <mutex>

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
        (game::IMqNetPlayerVftable::SendNetMessage)sendMessage,
        (game::IMqNetPlayerVftable::ReceiveMessage)receiveMessage,
        (game::IMqNetPlayerVftable::SetNetSystem)setNetSystem,
        (game::IMqNetPlayerVftable::Method8)method8,
    };

    this->vftable = &vftable;
}

CNetCustomPlayer::~CNetCustomPlayer()
{
    if (m_system) {
        playerLog("CNetCustomPlayer delete netSystem");
        m_system->vftable->destructor(m_system, 1);
    }

    if (m_reception) {
        playerLog("CNetCustomPlayer delete netReception");
        m_reception->vftable->destructor(m_reception, 1);
    }
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

void CNetCustomPlayer::setSystem(game::IMqNetSystem* value)
{
    m_system = value;
}

game::IMqNetReception* CNetCustomPlayer::getReception() const
{
    return m_reception;
}

void CNetCustomPlayer::setReception(game::IMqNetReception* value)
{
    m_reception = value;
}

const std::string& CNetCustomPlayer::getName() const
{
    return m_name;
}

void CNetCustomPlayer::setName(const char* value)
{
    m_name = value;
}

std::uint32_t CNetCustomPlayer::getId() const
{
    return m_id;
}

void CNetCustomPlayer::setId(std::uint32_t value)
{
    m_id = value;
}

void __fastcall CNetCustomPlayer::destructor(CNetCustomPlayer* thisptr, int /*%edx*/, char flags)
{
    playerLog("CNetCustomPlayer d-tor called");

    thisptr->~CNetCustomPlayer();

    if (flags & 1) {
        playerLog("CNetCustomPlayer free memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

game::String* __fastcall CNetCustomPlayer::getName(CNetCustomPlayer* thisptr,
                                                   int /*%edx*/,
                                                   game::String* string)
{
    playerLog(fmt::format("CNetCustomPlayer {:s} getName", thisptr->m_name));
    game::StringApi::get().initFromString(string, thisptr->m_name.c_str());
    return string;
}

int __fastcall CNetCustomPlayer::getNetId(CNetCustomPlayer* thisptr, int /*%edx*/)
{
    playerLog(fmt::format("CNetCustomPlayer {:s} getNetId 0x{:x}", thisptr->m_name, thisptr->m_id));
    return static_cast<int>(thisptr->m_id);
}

game::IMqNetSession* __fastcall CNetCustomPlayer::getSession(CNetCustomPlayer* thisptr,
                                                             int /*%edx*/)
{
    playerLog(fmt::format("CNetCustomPlayer {:s} getSession", thisptr->m_name));
    return thisptr->m_session;
}

int __fastcall CNetCustomPlayer::getMessageCount(CNetCustomPlayer* thisptr, int /*%edx*/)
{
    playerLog(fmt::format("CNetCustomPlayer {:s} getMessageCount", thisptr->m_name));
    return 1;
}

bool __fastcall CNetCustomPlayer::sendMessage(CNetCustomPlayer* thisptr,
                                              int /*%edx*/,
                                              int idTo,
                                              const game::NetMessageHeader* message)
{
    playerLog(fmt::format("CNetCustomPlayer {:s} sendMessage '{:s}' to {:d}", thisptr->m_name,
                          message->messageClassName, idTo));
    return true;
}

game::ReceiveMessageResult __fastcall CNetCustomPlayer::receiveMessage(
    CNetCustomPlayer* thisptr,
    int /*%edx*/,
    int* idFrom,
    game::NetMessageHeader* buffer)
{
    playerLog(fmt::format("CNetCustomPlayer {:s} receiveMessage", thisptr->m_name));
    return game::ReceiveMessageResult::NoMessages;
}

void __fastcall CNetCustomPlayer::setNetSystem(CNetCustomPlayer* thisptr,
                                               int /*%edx*/,
                                               game::IMqNetSystem* netSystem)
{
    playerLog(fmt::format("CNetCustomPlayer {:s} setNetSystem", thisptr->m_name));
    if (thisptr->m_system != netSystem) {
        if (thisptr->m_system) {
            thisptr->m_system->vftable->destructor(thisptr->m_system, 1);
        }
        thisptr->m_system = netSystem;
    }
}

int __fastcall CNetCustomPlayer::method8(CNetCustomPlayer* thisptr, int /*%edx*/, int a2)
{
    playerLog(fmt::format("CNetCustomPlayer {:s} method8", thisptr->m_name));
    return a2;
}

void playerLog(const std::string& message)
{
    static std::mutex logMutex;
    std::lock_guard lock(logMutex);

    logDebug("lobby.log", message);
}

} // namespace hooks
