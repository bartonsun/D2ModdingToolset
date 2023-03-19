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

#include "netcustomsession.h"
#include "d2string.h"
#include "lobbyclient.h"
#include "log.h"
#include "mempool.h"
#include "mqnetsystem.h"
#include "netcustomplayerclient.h"
#include "netcustomplayerserver.h"
#include "netcustomservice.h"
#include <atomic>
#include <chrono>
#include <fmt/format.h>
#include <thread>

namespace hooks {

CNetCustomSession* CNetCustomSession::create(CNetCustomService* service,
                                             const char* name,
                                             bool isHost)
{
    auto session = (CNetCustomSession*)game::Memory::get().allocate(sizeof(CNetCustomSession));
    new (session) CNetCustomSession(service, name, isHost);
    return session;
}

CNetCustomSession::CNetCustomSession(CNetCustomService* service, const char* name, bool isHost)
    : m_name{name}
    , m_service{service}
    , m_server{nullptr}
    , m_maxPlayers{2}
    , m_isHost{isHost}
{
    static game::IMqNetSessionVftable vftable = {
        (game::IMqNetSessionVftable::Destructor)destructor,
        (game::IMqNetSessionVftable::GetName)(GetName)getName,
        (game::IMqNetSessionVftable::GetClientCount)getClientCount,
        (game::IMqNetSessionVftable::GetMaxClients)getMaxClients,
        (game::IMqNetSessionVftable::GetPlayers)getPlayers,
        (game::IMqNetSessionVftable::CreateClient)createClient,
        (game::IMqNetSessionVftable::CreateServer)createServer,
    };

    this->vftable = &vftable;
}

CNetCustomService* CNetCustomSession::getService() const
{
    return m_service;
}

const std::string& CNetCustomSession::getName() const
{
    return m_name;
}

bool CNetCustomSession::isHost() const
{
    return m_isHost;
}

CNetCustomPlayerServer* CNetCustomSession::getServer() const
{
    return m_server;
}

void CNetCustomSession::setServer(CNetCustomPlayerServer* value)
{
    m_server = value;
}

CNetCustomPlayerClient* CNetCustomSession::getHostPlayer() const
{
    if (m_players.empty()) {
        logDebug("lobby.log", "Host player is null !!!");
        return nullptr;
    }

    return m_players.front();
}

void CNetCustomSession::addPlayer(CNetCustomPlayerClient* value)
{
    logDebug("lobby.log", "CNetCustomSession addPlayer called");
    m_players.push_back(value);
}

bool CNetCustomSession::setMaxPlayers(int maxPlayers)
{
    if (maxPlayers < 1 || maxPlayers > 4) {
        return false;
    }

    // -1 because room already have moderator.
    const auto result{tryChangeRoomPublicSlots(maxPlayers - 1)};
    if (result) {
        m_maxPlayers = maxPlayers;
    }

    return result;
}

void __fastcall CNetCustomSession::destructor(CNetCustomSession* thisptr, int /*%edx*/, char flags)
{
    logDebug("lobby.log", "CNetCustomSession d-tor called");

    thisptr->~CNetCustomSession();

    if (flags & 1) {
        logDebug("lobby.log", "CNetCustomSession d-tor frees memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

game::String* __fastcall CNetCustomSession::getName(CNetCustomSession* thisptr,
                                                    int /*%edx*/,
                                                    game::String* sessionName)
{
    logDebug("lobby.log", "CNetCustomSession getName called");

    game::StringApi::get().initFromString(sessionName, thisptr->m_name.c_str());
    return sessionName;
}

int __fastcall CNetCustomSession::getClientCount(CNetCustomSession* thisptr, int /*%edx*/)
{
    logDebug("lobby.log", "CNetCustomSession getClientCount called");
    return (int)thisptr->m_players.size();
}

int __fastcall CNetCustomSession::getMaxClients(CNetCustomSession* thisptr, int /*%edx*/)
{
    logDebug("lobby.log", fmt::format("CNetCustomSession getMaxClients called. Max clients: {:d}",
                                      thisptr->m_maxPlayers));
    return thisptr->m_maxPlayers;
}

void __fastcall CNetCustomSession::getPlayers(CNetCustomSession* thisptr,
                                              int /*%edx*/,
                                              game::List<game::IMqNetPlayerEnum*>* players)
{
    logDebug("lobby.log", "CNetCustomSession getPlayers called");
}

void __fastcall CNetCustomSession::createClient(CNetCustomSession* thisptr,
                                                int /*%edx*/,
                                                game::IMqNetPlayerClient** client,
                                                game::IMqNetSystem* netSystem,
                                                game::IMqNetReception* reception,
                                                const char* clientName)
{
    logDebug("lobby.log", fmt::format("CNetCustomSession createClient '{:s}' called", clientName));

    // Finalize player setup here when we know all the settings
    auto host = thisptr->getHostPlayer();
    host->setName(clientName);
    host->setSystem(netSystem);
    host->setReception(reception);

    *client = (game::IMqNetPlayerClient*)host;
}

void __fastcall CNetCustomSession::createServer(CNetCustomSession* thisptr,
                                                int /*%edx*/,
                                                game::IMqNetPlayerServer** server,
                                                game::IMqNetSystem* netSystem,
                                                game::IMqNetReception* reception)
{
    logDebug("lobby.log", "CNetCustomSession createServer called");

    *server = nullptr;
    if (!thisptr->m_server) {
        logDebug("lobby.log", "Player server is null !!!");
        return;
    }

    auto player = thisptr->m_server;
    player->setSystem(netSystem);
    player->setReception(reception);

    *server = (game::IMqNetPlayerServer*)player;
}

} // namespace hooks
