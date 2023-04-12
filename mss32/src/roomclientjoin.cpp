/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Vladimir Makeev.
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

#include "roomclientjoin.h"
#include "log.h"
#include "menucustomlobby.h"
#include "menuphase.h"
#include "midgard.h"
#include "netcustomplayerclient.h"
#include "netcustomservice.h"
#include "netcustomsession.h"
#include "netmessages.h"
#include "settings.h"
#include <fmt/format.h>

namespace hooks {

static CMenuCustomLobby* menuLobby{nullptr};

static void registerClientPlayerAndJoin()
{
    using namespace game;

    auto& midgardApi = CMidgardApi::get();
    auto midgard = midgardApi.instance();
    auto netService = getNetService();

    // Create player using session
    logDebug("roomJoin.log", "Try create net client using midgard");
    auto playerId = midgardApi.createNetClient(midgard, netService->getAccountName().c_str(),
                                               false);

    logDebug("roomJoin.log", fmt::format("Check net client id 0x{:x}", playerId));
    if (playerId == 0) {
        // Network id of zero is invalid
        logDebug("roomJoin.log", "Net client has zero id");
        customLobbyProcessJoinError(menuLobby, "Created net client has net id 0");
        return;
    }

    logDebug("roomJoin.log", "Set menu phase as a net client proxy");

    auto menuPhase = menuLobby->menuBaseData->menuPhase;
    // Set menu phase as net proxy
    midgardApi.setClientsNetProxy(midgard, menuPhase);

    // Get max players from session
    auto netSession{netService->getSession()};
    auto maxPlayers = netSession->vftable->getMaxClients(netSession);

    logDebug("roomJoin.log", fmt::format("Get max number of players in session: {:d}", maxPlayers));

    menuPhase->data->maxPlayers = maxPlayers;

    CMenusReqVersionMsg requestVersion;
    requestVersion.vftable = NetMessagesApi::getMenusReqVersionVftable();

    logDebug("roomJoin.log", "Send version request message to server");
    // Send CMenusReqVersionMsg to server
    // If failed, hide wait message and show error, enable join
    if (!midgardApi.sendNetMsgToServer(midgard, &requestVersion)) {
        customLobbyProcessJoinError(menuLobby, "Could not request game version from server");
        return;
    }

    // Response will be handled by CMenuCustomLobby menuGameVersionMsgHandler
    logDebug("roomJoin.log", "Request sent, wait for callback");
}

void customLobbyProcessJoin(CMenuCustomLobby* menu,
                            const char* roomName,
                            const SLNet::RakNetGUID& serverGuid)
{
    logDebug("roomJoin.log", "Unsubscribe from rooms list updates while joining the room");

    // Unsubscribe from rooms list notification while we joining the room
    // Remove timer event
    game::UiEventApi::get().destructor(&menu->roomsListEvent);

    // Disconnect ui-related rooms callbacks
    getNetService()->removeRoomsCallback(menu->roomsCallbacks.get());
    menu->roomsCallbacks.reset(nullptr);

    using namespace game;

    menuLobby = menu;

    // Create session ourselves
    logDebug("roomJoin.log", fmt::format("Process join to {:s}", roomName));

    auto netService = getNetService();
    logDebug("roomJoin.log", fmt::format("Get netService {:p}", (void*)netService));

    if (!netService) {
        customLobbyProcessJoinError(menu, "Net service is null");
        return;
    }

    auto netSession = CNetCustomSession::create(netService, roomName, serverGuid);

    logDebug("roomJoin.log", fmt::format("Created netSession {:p}", (void*)netSession));

    // If failed, hide wait message and show error, enable join
    if (!netSession) {
        customLobbyProcessJoinError(menu, "Could not create net session");
        return;
    }

    // Set net session to midgard
    auto& midgardApi = CMidgardApi::get();

    auto midgard = midgardApi.instance();
    auto currentSession{midgard->data->netSession};
    if (currentSession) {
        currentSession->vftable->destructor(currentSession, 1);
    }

    logDebug("roomJoin.log", "Set netSession to midgard");
    midgard->data->netSession = netSession;
    netService->setSession(netSession);

    logDebug("roomJoin.log", "Mark self as client");
    // Mark self as client
    midgard->data->host = false;

    logDebug("roomJoin.log", "Create player client beforehand");

    registerClientPlayerAndJoin();
}

} // namespace hooks
