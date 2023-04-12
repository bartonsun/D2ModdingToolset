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

#include "roomservercreation.h"
#include "dialoginterf.h"
#include "editboxinterf.h"
#include "log.h"
#include "mempool.h"
#include "menubase.h"
#include "menuflashwait.h"
#include "netcustomplayerclient.h"
#include "netcustomplayerserver.h"
#include "netcustomservice.h"
#include "netcustomsession.h"
#include "originalfunctions.h"
#include "roomscallback.h"
#include "settings.h"
#include "utils.h"
#include <fmt/format.h>

namespace hooks {

game::CMenuFlashWait* menuWaitServer{nullptr};
game::CMenuBase* menuBase{nullptr};
static bool loadingScenario{false};
static std::string roomPassword{};

static void hideWaitMenu()
{
    if (menuWaitServer) {
        hideInterface(menuWaitServer);
        menuWaitServer->vftable->destructor(menuWaitServer, 1);
        menuWaitServer = nullptr;
    }
}

static void serverCreationError(const std::string& message)
{
    hideWaitMenu();
    showMessageBox(message);
}

/**
 * Handles response to room creation request.
 * Creates host player.
 */
class RoomsCreateServerCallback : public SLNet::RoomsCallback
{
public:
    RoomsCreateServerCallback() = default;
    ~RoomsCreateServerCallback() override = default;

    void CreateRoom_Callback(const SLNet::SystemAddress& senderAddress,
                             SLNet::CreateRoom_Func* callResult) override
    {
        logDebug("lobby.log", "Room creation response received");

        // Unsubscribe from callbacks
        getNetService()->removeRoomsCallback(this);

        if (callResult->resultCode != SLNet::REC_SUCCESS) {
            auto result{SLNet::RoomsErrorCodeDescription::ToEnglish(callResult->resultCode)};
            const auto msg{fmt::format("Could not create a room.\nReason: {:s}", result)};

            logDebug("lobby.log", msg);
            serverCreationError(msg);
            return;
        }

        hideWaitMenu();
        if (menuBase) {
            const auto& fn = getOriginalFunctions();

            if (loadingScenario) {
                logDebug("lobby.log", "Proceed to next screen as CMenuLoadSkirmishMulti");
                fn.menuLoadSkirmishMultiLoadScenario((game::CMenuLoad*)menuBase);
            } else {
                logDebug("lobby.log", "Proceed to next screen as CMenuNewSkirmishMulti");
                fn.menuNewSkirmishLoadScenario(menuBase);
            }
        } else {
            logDebug("lobby.log", "MenuBase is null somehow!");
        }
    }
};

static RoomsCreateServerCallback roomServerCallback;

static void createSession(const char* sessionName)
{
    logDebug("lobby.log", "Create session");

    auto service = getNetService();
    service->setSession(CNetCustomSession::create(service, sessionName, service->getPeerGuid()));

    logDebug("lobby.log", "Create player server");

    service->addRoomsCallback(&roomServerCallback);

    const char* password{roomPassword.empty() ? nullptr : roomPassword.c_str()};

    // Request room creation and wait for lobby server response
    if (!service->createRoom(password)) {
        serverCreationError("Failed to request room creation");
        return;
    }

    logDebug("lobby.log", "Waiting for room creation response");
}

void startRoomAndServerCreation(game::CMenuBase* menu, bool loadScenario)
{
    using namespace game;

    menuWaitServer = (CMenuFlashWait*)Memory::get().allocate(sizeof(CMenuFlashWait));
    CMenuFlashWaitApi::get().constructor(menuWaitServer);

    showInterface(menuWaitServer);

    logDebug("lobby.log", "Create session and player server");
    menuBase = menu;
    loadingScenario = loadScenario;

    auto& menuApi = CMenuBaseApi::get();
    auto& dialogApi = CDialogInterfApi::get();
    auto dialog = menuApi.getDialogInterface(menu);
    auto password = dialogApi.findEditBox(dialog, "EDIT_PASSWORD");

    const auto& passwordString{password->data->editBoxData.inputString};
    if (!passwordString.length) {
        roomPassword.clear();
    } else {
        roomPassword = passwordString.string;
    }

    auto editGame = dialogApi.findEditBox(dialog, "EDIT_GAME");
    createSession(editGame->data->editBoxData.inputString.string);
}

} // namespace hooks
