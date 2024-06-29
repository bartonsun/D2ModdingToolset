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

#include "menucustomlobby.h"
#include "autodialog.h"
#include "button.h"
#include "dialoginterf.h"
#include "dynamiccast.h"
#include "globaldata.h"
#include "image2fill.h"
#include "image2outline.h"
#include "image2text.h"
#include "interfmanager.h"
#include "listbox.h"
#include "log.h"
#include "mempool.h"
#include "menuphase.h"
#include "midgard.h"
#include "midgardmsgbox.h"
#include "netcustomplayerclient.h"
#include "netcustomsession.h"
#include "netmessages.h"
#include "netmsgcallbacks.h"
#include "netmsgmapentry.h"
#include "originalfunctions.h"
#include "racelist.h"
#include "textboxinterf.h"
#include "textids.h"
#include "uimanager.h"
#include "utils.h"
#include <chrono>
#include <fmt/chrono.h>
#include <thread>

namespace hooks {

CMenuCustomLobby::CMenuCustomLobby(game::CMenuPhase* menuPhase)
    : CMenuCustomBase(this)
    , roomsCallbacks(this)
    , netMsgEntryData(nullptr)
    , roomPasswordDialog(nullptr)
{
    using namespace game;

    const auto& menuBaseApi = CMenuBaseApi::get();

    logDebug("transitions.log", "Call CMenuBase c-tor for CMenuCustomLobby");
    menuBaseApi.constructor(this, menuPhase);

    static RttiInfo<CMenuBaseVftable> rttiInfo = {};
    if (rttiInfo.locator == nullptr) {
        // Reuse object locator for our custom this.
        // We only need it for dynamic_cast<CAnimInterf>() to work properly without exceptions
        // when the game exits main loop and checks for current CInterface.
        // See DestroyAnimInterface() in IDA.
        replaceRttiInfo(rttiInfo, this->vftable);
        rttiInfo.vftable.destructor = (CInterfaceVftable::Destructor)&destructor;
    }
    this->vftable = &rttiInfo.vftable;

    logDebug("transitions.log", "Call createMenu for CMenuCustomLobby");
    menuBaseApi.createMenu(this, dialogName);

    initializeButtonsHandlers();

    auto service = getNetService();
    updateAccountText(service->getAccountName().c_str());

    // Connect ui-related rooms callbacks
    service->addRoomsCallback(&roomsCallbacks);

    // Request rooms list as soon as possible, no need to wait for event
    service->searchRooms();

    // Add timer event that will send rooms list requests every 5 seconds
    createTimerEvent(&roomsListEvent, this, roomsListSearchHandler, roomListUpdateInterval);

    // Setup ingame net message callbacks
    {
        logDebug("netCallbacks.log", "Allocate entry data");

        auto& netApi = NetMsgApi::get();
        netApi.allocateEntryData(this->menuBaseData->menuPhase, &this->netMsgEntryData);

        auto& entryApi = CNetMsgMapEntryApi::get();

        logDebug("netCallbacks.log", "Allocate CMenusAnsInfoMsg entry");

        auto infoCallback = (CNetMsgMapEntryApi::Api::MenusAnsInfoCallback)ansInfoMsgHandler;
        auto entry = entryApi.allocateMenusAnsInfoEntry(this, infoCallback);

        logDebug("netCallbacks.log", "Add CMenusAnsInfoMsg entry");
        netApi.addEntry(this->netMsgEntryData, entry);

        logDebug("netCallbacks.log", "Allocate CGameVersionMsg entry");

        auto versionCallback = (CNetMsgMapEntryApi::Api::GameVersionCallback)gameVersionMsgHandler;
        entry = entryApi.allocateGameVersionEntry(this, versionCallback);

        logDebug("netCallbacks.log", "Add CGameVersionMsg entry");
        netApi.addEntry(this->netMsgEntryData, entry);
    }
}

CMenuCustomLobby ::~CMenuCustomLobby()
{
    using namespace game;

    hideRoomPasswordDialog();

    if (netMsgEntryData) {
        logDebug("transitions.log", "Delete custom lobby menu netMsgEntryData");
        NetMsgApi::get().freeEntryData(netMsgEntryData);
    }

    logDebug("transitions.log", "Delete rooms list event");
    UiEventApi::get().destructor(&roomsListEvent);

    auto service = getNetService();
    if (service) {
        service->removeRoomsCallback(&roomsCallbacks);
    }

    CMenuBaseApi::get().destructor(this);
}

void CMenuCustomLobby::updateAccountText(const char* accountName)
{
    using namespace game;

    auto& menuBase = CMenuBaseApi::get();
    auto& dialogApi = CDialogInterfApi::get();
    auto dialog = menuBase.getDialogInterface(this);
    auto textBox = dialogApi.findTextBox(dialog, "TXT_NICKNAME");

    if (accountName) {
        auto text{fmt::format("\\fLarge;\\vC;\\hC;{:s}", accountName)};
        CTextBoxInterfApi::get().setString(textBox, text.c_str());
        return;
    }

    CTextBoxInterfApi::get().setString(textBox, "");
}

void __fastcall CMenuCustomLobby::destructor(CMenuCustomLobby* thisptr, int /*%edx*/, char flags)
{
    thisptr->~CMenuCustomLobby();

    if (flags & 1) {
        logDebug("transitions.log", "Free CMenuCustomLobby memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

void CMenuCustomLobby::showRoomPasswordDialog()
{
    using namespace game;

    if (roomPasswordDialog) {
        logDebug("lobby.log", "Error showing room-password dialog that is already shown");
        return;
    }

    roomPasswordDialog = (CRoomPasswordInterf*)game::Memory::get().allocate(
        sizeof(CRoomPasswordInterf));
    showInterface(new (roomPasswordDialog) CRoomPasswordInterf(this));
}

void CMenuCustomLobby::hideRoomPasswordDialog()
{
    if (roomPasswordDialog) {
        hideInterface(roomPasswordDialog);
        roomPasswordDialog->vftable->destructor(roomPasswordDialog, 1);
        roomPasswordDialog = nullptr;
    }
}

void __fastcall CMenuCustomLobby::registerAccountBtnHandler(CMenuCustomLobby*, int /*%edx*/)
{
    // TODO: remove
}

void __fastcall CMenuCustomLobby::loginAccountBtnHandler(CMenuCustomLobby*, int /*%edx*/)
{
    // TODO: remove
}

void __fastcall CMenuCustomLobby::logoutAccountBtnHandler(CMenuCustomLobby*, int /*%edx*/)
{
    logDebug("lobby.log", "User logging out");
    getNetService()->logoutAccount();
}

void __fastcall CMenuCustomLobby::roomsListSearchHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    getNetService()->searchRooms();
}

void __fastcall CMenuCustomLobby::createRoomBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    using namespace game;

    auto menuPhase = thisptr->menuBaseData->menuPhase;
    menuPhase->data->currentPhase = MenuPhase::Multi;
    menuPhase->data->host = true;

    logDebug("transitions.log",
             "Create room, pretend we are in CMenuMulti, transition to CMenuNewSkirmishMulti");
    game::CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::Multi2NewSkirmish);
}

void __fastcall CMenuCustomLobby::loadBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    using namespace game;

    auto menuPhase = thisptr->menuBaseData->menuPhase;
    menuPhase->data->currentPhase = MenuPhase::Multi;
    menuPhase->data->host = true;

    logDebug("transitions.log",
             "Create room, pretend we are in CMenuMulti, transition to CMenuLoadSkirmishMulti");
    game::CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::Multi2LoadSkirmish);
}

const CMenuCustomLobby::RoomInfo* CMenuCustomLobby::getSelectedRoom()
{
    using namespace game;

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBox = CDialogInterfApi::get().findListBox(dialog, "LBOX_ROOMS");
    auto index = (size_t)CListBoxInterfApi::get().selectedIndex(listBox);
    return index < rooms.size() ? &rooms[index] : nullptr;
}

void __fastcall CMenuCustomLobby::joinRoomBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    using namespace game;

    logDebug("transitions.log", "Join room button pressed");

    auto room = thisptr->getSelectedRoom();
    if (!room) {
        return;
    }

    // Do fast check if room can be joined
    if (room->usedSlots >= room->totalSlots) {
        // Could not join game. Host did not report any available race.
        showMessageBox(getInterfaceText("X005TA0886"));
        return;
    }

    if (room->gameFilesHash != getNetService()->getGameFilesHash()) {
        auto message{getInterfaceText(textIds().lobby.checkFilesFailed.c_str())};
        if (message.empty()) {
            message = "Unable to join the room because the owner's game files are different.";
        }
        showMessageBox(message);
        return;
    }

    if (room->password.empty()) {
        getNetService()->joinRoom(room->id);
        thisptr->showWaitDialog();
    } else {
        // TODO: stop refresh timer or preserve whole room info (or pass the room info into the
        // dialog ctor)
        thisptr->joiningRoomId = room->id;
        thisptr->showRoomPasswordDialog();
    }
}

void __fastcall CMenuCustomLobby::listBoxDisplayHandler(CMenuCustomLobby* thisptr,
                                                        int /*%edx*/,
                                                        game::ImagePointList* contents,
                                                        const game::CMqRect* lineArea,
                                                        int index,
                                                        bool selected)
{
    if (thisptr->rooms.empty()) {
        return;
    }

    using namespace game;

    auto& createFreePtr = SmartPointerApi::get().createOrFree;
    auto& imageApi = CImage2TextApi::get();

    const auto width = lineArea->right - lineArea->left;
    const auto height = lineArea->bottom - lineArea->top;
    const auto& room = thisptr->rooms[(size_t)index % thisptr->rooms.size()];

    // Width of table column border image in pixels
    const int columnBorderWidth{9};
    // 'Host' column with in pixels
    const int hostTextWidth{122};

    int offset = 0;

    // Host name
    {
        auto hostText = (CImage2Text*)Memory::get().allocate(sizeof(CImage2Text));
        imageApi.constructor(hostText, hostTextWidth, height);
        imageApi.setText(hostText, fmt::format("\\vC;{:s}", room.hostName).c_str());

        ImagePtrPointPair pair{};
        createFreePtr((SmartPointer*)&pair.first, hostText);
        pair.second.x = offset + 5;
        pair.second.y = 0;

        ImagePointListApi::get().add(contents, &pair);
        createFreePtr((SmartPointer*)&pair.first, nullptr);

        offset += hostTextWidth + columnBorderWidth;
    }

    // 'Description' column width in pixels
    const int descriptionWidth{452};

    // Room description
    {
        auto description = (CImage2Text*)Memory::get().allocate(sizeof(CImage2Text));
        imageApi.constructor(description, descriptionWidth, height);
        imageApi.setText(description, fmt::format("\\vC;{:s}", room.name).c_str());

        ImagePtrPointPair pair{};
        createFreePtr((SmartPointer*)&pair.first, description);
        pair.second.x = offset + 5;
        pair.second.y = 0;

        ImagePointListApi::get().add(contents, &pair);
        createFreePtr((SmartPointer*)&pair.first, nullptr);

        offset += descriptionWidth + columnBorderWidth;
    }

    // '#' column width
    const int playerCountWidth{44};

    // Number of players in room
    {
        auto playerCount = (CImage2Text*)Memory::get().allocate(sizeof(CImage2Text));
        imageApi.constructor(playerCount, playerCountWidth, height);
        imageApi
            .setText(playerCount,
                     fmt::format("\\vC;\\hC;{:d}/{:d}", room.usedSlots, room.totalSlots).c_str());

        ImagePtrPointPair pair{};
        createFreePtr((SmartPointer*)&pair.first, playerCount);
        pair.second.x = offset;
        pair.second.y = 0;

        ImagePointListApi::get().add(contents, &pair);
        createFreePtr((SmartPointer*)&pair.first, nullptr);

        offset += playerCountWidth + columnBorderWidth;
    }

    // Password protection status
    if (!room.password.empty()) {
        auto lockImage{AutoDialogApi::get().loadImage("ROOM_PROTECTED")};
        ImagePtrPointPair pair{};
        createFreePtr((SmartPointer*)&pair.first, lockImage);
        // Adjust lock image position to match with lock image in column header
        pair.second.x = offset + 3;
        pair.second.y = 1;

        ImagePointListApi::get().add(contents, &pair);
        createFreePtr((SmartPointer*)&pair.first, nullptr);
    }

    // Outline for selected element
    if (selected) {
        auto outline = (CImage2Outline*)Memory::get().allocate(sizeof(CImage2Outline));

        CMqPoint size{};
        size.x = width;
        size.y = height;

        game::Color color{};
        // 0x0 - transparent, 0xff - opaque
        std::uint32_t opacity{0xff};
        CImage2OutlineApi::get().constructor(outline, &size, &color, opacity);

        ImagePtrPointPair pair{};
        createFreePtr((SmartPointer*)&pair.first, outline);
        pair.second.x = 0;
        pair.second.y = 0;

        ImagePointListApi::get().add(contents, &pair);
        createFreePtr((SmartPointer*)&pair.first, nullptr);
    }
}

bool __fastcall CMenuCustomLobby::ansInfoMsgHandler(CMenuCustomLobby* menu,
                                                    int /*%edx*/,
                                                    const game::CMenusAnsInfoMsg* message,
                                                    std::uint32_t idFrom)
{
    logDebug("netCallbacks.log", fmt::format("CMenusAnsInfoMsg received from 0x{:x}", idFrom));

    if (message->raceIds.length == 0) {
        menu->processJoinError("No available races");
        return true;
    }

    using namespace game;

    auto menuPhase = menu->menuBaseData->menuPhase;
    auto globalData = *GlobalDataApi::get().getGlobalData();
    auto racesTable = globalData->raceCategories;
    auto& findRaceById = LRaceCategoryTableApi::get().findCategoryById;
    auto& listApi = RaceCategoryListApi::get();

    auto raceList = &menuPhase->data->races;
    listApi.freeNodes(raceList);

    for (auto node = message->raceIds.head->next; node != message->raceIds.head;
         node = node->next) {
        const int raceId = static_cast<const int>(node->data);

        LRaceCategory category{};
        findRaceById(racesTable, &category, &raceId);

        listApi.add(raceList, &category);
    }

    menuPhase->data->unknown8 = false;
    menuPhase->data->suggestedLevel = message->suggestedLevel;

    allocateString(&menuPhase->data->scenarioName, message->scenarioName);
    allocateString(&menuPhase->data->scenarioDescription, message->scenarioDescription);

    logDebug("netCallbacks.log", "Switch to CMenuLobbyJoin (I hope)");
    // Here we can set next menu transition, there is no need to hide wait message,
    // it will be hidden from the destructor
    // Pretend we are in transition 15, after CMenuSession, transition to CMenuLobbyJoin
    menuPhase->data->currentPhase = MenuPhase::Session2LobbyJoin;
    menuPhase->data->host = false;

    logDebug("transitions.log",
             "Joining room, pretend we are in phase 15, transition to CMenuLobbyJoin");
    game::CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::Session2LobbyJoin);

    return true;
}

bool __fastcall CMenuCustomLobby::gameVersionMsgHandler(CMenuCustomLobby* menu,
                                                        int /*%edx*/,
                                                        const game::CGameVersionMsg* message,
                                                        std::uint32_t idFrom)
{
    // Check server version
    if (message->gameVersion != 40) {
        // "You are trying to join a game with a newer or an older version of the game."
        menu->processJoinError(getInterfaceText("X006ta0008").c_str());
        return true;
    }

    using namespace game;

    CMenusReqInfoMsg requestInfo;
    requestInfo.vftable = NetMessagesApi::getMenusReqInfoVftable();

    auto& midgardApi = CMidgardApi::get();
    auto midgard = midgardApi.instance();

    midgardApi.sendNetMsgToServer(midgard, &requestInfo);

    auto msg{fmt::format("CGameVersionMsg received from 0x{:x}", idFrom)};
    logDebug("netCallbacks.log", msg);

    return true;
}

void __fastcall CMenuCustomLobby::backBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    using namespace game;

    auto message = getInterfaceText(textIds().lobby.confirmBack.c_str());
    if (message.empty()) {
        message = "Do you really want to exit the lobby?";
    }

    auto handler = (CConfirmBackMsgBoxButtonHandler*)Memory::get().allocate(
        sizeof(CConfirmBackMsgBoxButtonHandler));
    new (handler) CConfirmBackMsgBoxButtonHandler(thisptr);

    showMessageBox(message, handler, true);
}

void CMenuCustomLobby::initializeButtonsHandlers()
{
    using namespace game;

    const auto& menuBaseApi = CMenuBaseApi::get();
    const auto& dialogApi = CDialogInterfApi::get();
    const auto& buttonApi = CButtonInterfApi::get();
    const auto& listBoxApi = CListBoxInterfApi::get();
    const auto freeFunctor = SmartPointerApi::get().createOrFreeNoDtor;

    auto dialog = menuBaseApi.getDialogInterface(this);

    SmartPointer functor;
    auto callback = (CMenuBaseApi::Api::ButtonCallback)backBtnHandler;
    menuBaseApi.createButtonFunctor(&functor, 0, this, &callback);
    buttonApi.assignFunctor(dialog, "BTN_BACK", dialogName, &functor, 0);
    freeFunctor(&functor, nullptr);

    callback = (CMenuBaseApi::Api::ButtonCallback)registerAccountBtnHandler;
    menuBaseApi.createButtonFunctor(&functor, 0, this, &callback);
    buttonApi.assignFunctor(dialog, "BTN_REGISTER", dialogName, &functor, 0);
    freeFunctor(&functor, nullptr);

    callback = (CMenuBaseApi::Api::ButtonCallback)loginAccountBtnHandler;
    menuBaseApi.createButtonFunctor(&functor, 0, this, &callback);
    buttonApi.assignFunctor(dialog, "BTN_LOGIN", dialogName, &functor, 0);
    freeFunctor(&functor, nullptr);

    callback = (CMenuBaseApi::Api::ButtonCallback)logoutAccountBtnHandler;
    menuBaseApi.createButtonFunctor(&functor, 0, this, &callback);
    buttonApi.assignFunctor(dialog, "BTN_LOGOUT", dialogName, &functor, 0);
    freeFunctor(&functor, nullptr);

    callback = (CMenuBaseApi::Api::ButtonCallback)createRoomBtnHandler;
    menuBaseApi.createButtonFunctor(&functor, 0, this, &callback);
    buttonApi.assignFunctor(dialog, "BTN_CREATE", dialogName, &functor, 0);
    freeFunctor(&functor, nullptr);

    callback = (CMenuBaseApi::Api::ButtonCallback)loadBtnHandler;
    menuBaseApi.createButtonFunctor(&functor, 0, this, &callback);
    buttonApi.assignFunctor(dialog, "BTN_LOAD", dialogName, &functor, 0);
    freeFunctor(&functor, nullptr);

    callback = (CMenuBaseApi::Api::ButtonCallback)joinRoomBtnHandler;
    menuBaseApi.createButtonFunctor(&functor, 0, this, &callback);
    buttonApi.assignFunctor(dialog, "BTN_JOIN", dialogName, &functor, 0);
    freeFunctor(&functor, nullptr);

    // TODO: completely remove the button
    auto buttonRegister = dialogApi.findButton(dialog, "BTN_REGISTER");
    if (buttonRegister) {
        buttonRegister->vftable->setEnabled(buttonRegister, false);
    }

    // TODO: completely remove the button
    auto buttonLogin = dialogApi.findButton(dialog, "BTN_LOGIN");
    if (buttonLogin) {
        buttonLogin->vftable->setEnabled(buttonLogin, false);
    }

    // TODO: completely remove the button
    auto buttonLogout = dialogApi.findButton(dialog, "BTN_LOGOUT");
    if (buttonLogout) {
        buttonLogout->vftable->setEnabled(buttonLogout, false);
    }

    auto listBoxRooms = dialogApi.findListBox(dialog, "LBOX_ROOMS");
    if (listBoxRooms) {
        auto callback = (CMenuBaseApi::Api::ListBoxDisplayCallback)listBoxDisplayHandler;
        menuBaseApi.createListBoxDisplayFunctor(&functor, 0, this, &callback);
        listBoxApi.assignDisplaySurfaceFunctor(dialog, "LBOX_ROOMS", dialogName, &functor);
        listBoxApi.setElementsTotal(listBoxRooms, 0);
        freeFunctor(&functor, nullptr);
    }
}

void CMenuCustomLobby::showError(const char* message)
{
    showMessageBox(fmt::format("\\fLarge;\\hC;Error\\fSmall;\n\n{:s}", message));
}

void CMenuCustomLobby::registerClientPlayerAndJoin()
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
        processJoinError("Created net client has net id 0");
        return;
    }

    logDebug("roomJoin.log", "Set menu phase as a net client proxy");

    auto menuPhase = menuBaseData->menuPhase;
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
        processJoinError("Could not request game version from server");
        return;
    }

    // Response will be handled by CMenuCustomLobby menuGameVersionMsgHandler
    logDebug("roomJoin.log", "Request sent, wait for callback");
}

void CMenuCustomLobby::processJoin(const char* roomName, const SLNet::RakNetGUID& serverGuid)
{
    using namespace game;

    // Create session ourselves
    logDebug("roomJoin.log", fmt::format("Process join to {:s}", roomName));

    auto netService = getNetService();
    logDebug("roomJoin.log", fmt::format("Get netService {:p}", (void*)netService));

    if (!netService) {
        processJoinError("Net service is null");
        return;
    }

    // TODO: netService->joinSession
    // TODO: fill in the password and use it to enter
    // TODO: move joining process inside joinSession method
    auto netSession = (CNetCustomSession*)game::Memory::get().allocate(sizeof(CNetCustomSession));
    new (netSession) CNetCustomSession(netService, roomName, "", serverGuid);

    logDebug("roomJoin.log", fmt::format("Created netSession {:p}", (void*)netSession));

    // If failed, hide wait message and show error, enable join
    if (!netSession) {
        processJoinError("Could not create net session");
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

void CMenuCustomLobby::setRoomsInfo(std::vector<RoomInfo>&& value)
{
    using namespace game;

    auto& menuBase = CMenuBaseApi::get();
    auto& dialogApi = CDialogInterfApi::get();
    auto dialog = menuBase.getDialogInterface(this);
    auto listBox = dialogApi.findListBox(dialog, "LBOX_ROOMS");
    auto& listBoxApi = CListBoxInterfApi::get();

    rooms = std::move(value);
    listBoxApi.setElementsTotal(listBox, (int)rooms.size());
}

void CMenuCustomLobby::processJoinError(const char* message)
{
    using namespace game;

    auto& menuBase = CMenuBaseApi::get();
    auto& dialogApi = CDialogInterfApi::get();
    auto dialog = menuBase.getDialogInterface(this);

    // TODO: check player entered a room in lobby server,
    // if so, leave it and enable join button in rooms callback
    // TODO: should be covered by wait dialog, do not disable-enable it
    auto buttonJoin = dialogApi.findButton(dialog, "BTN_JOIN");
    if (buttonJoin) {
        buttonJoin->vftable->setEnabled(buttonJoin, true);
    }

    showError(message);
}

void CMenuCustomLobby::RoomListCallbacks::JoinByFilter_Callback(
    const SLNet::SystemAddress&,
    SLNet::JoinByFilter_Func* callResult)
{
    menuLobby->hideWaitDialog();

    if (callResult->resultCode == SLNet::REC_SUCCESS) {
        auto& room = callResult->joinedRoomResult.roomDescriptor;
        auto roomName = room.GetProperty(DefaultRoomColumns::TC_ROOM_NAME)->c;

        logDebug("roomsCallbacks.log",
                 fmt::format("{:s} joined room",
                             callResult->joinedRoomResult.joiningMemberName.C_String()));

        const char* guidString{nullptr};

        auto& table = room.roomProperties;
        auto index = table.ColumnIndex(serverGuidColumnName);
        if (index != std::numeric_limits<unsigned int>::max()) {
            auto row = table.GetRowByIndex(0, nullptr);
            if (row) {
                guidString = row->cells[index]->c;
            }
        }

        // TODO: use guid of member with moderator role from room.roomMemberList, remove
        // serverGuidColumn
        // TODO: check if password can be transmitted using native RakNet instead of custom
        // column
        SLNet::RakNetGUID serverGuid{};
        if (!serverGuid.FromString(guidString)) {
            menuLobby->processJoinError("Could not get player server GUID");
            return;
        }

        menuLobby->processJoin(roomName, serverGuid);
        return;
    }

    auto error = SLNet::RoomsErrorCodeDescription::ToEnglish(callResult->resultCode);
    menuLobby->processJoinError(error);
}

void CMenuCustomLobby::RoomListCallbacks::SearchByFilter_Callback(
    const SLNet::SystemAddress&,
    SLNet::SearchByFilter_Func* callResult)
{
    if (callResult->resultCode != SLNet::REC_SUCCESS) {
        if (!getNetService()->loggedIn()) {
            // The error is expected, just silently remove the search event.
            game::UiEventApi::get().destructor(&menuLobby->roomsListEvent);
            return;
        }

        menuLobby->showError(SLNet::RoomsErrorCodeDescription::ToEnglish(callResult->resultCode));
        return;
    }

    // TODO: refactor the following mess

    auto& rooms = callResult->roomsOutput;
    const auto total{rooms.Size()};

    std::vector<RoomInfo> roomsInfo;
    roomsInfo.reserve(total);

    for (std::uint32_t i = 0; i < total; ++i) {
        auto room = rooms[i];

        auto name = room->GetProperty(DefaultRoomColumns::TC_ROOM_NAME)->c;
        SLNet::RakNetGUID hostGuid;
        const char* hostName{nullptr};

        // TODO: room->GetModerator
        auto& members = room->roomMemberList;
        for (std::uint32_t j = 0; members.Size(); ++j) {
            auto& member = members[j];

            if (member.roomMemberMode == RMM_MODERATOR) {
                hostGuid = member.guid;
                hostName = member.name.C_String();
                break;
            }
        }

        auto totalSlots = (int)room->GetProperty(DefaultRoomColumns::TC_TOTAL_PUBLIC_SLOTS)->i;
        auto usedSlots = (int)room->GetProperty(DefaultRoomColumns::TC_USED_PUBLIC_SLOTS)->i;

        const char* password{nullptr};

        auto& table = room->roomProperties;
        auto index = table.ColumnIndex(passwordColumnName);
        if (index != std::numeric_limits<unsigned int>::max()) {
            auto row = table.GetRowByIndex(0, nullptr);
            if (row) {
                password = row->cells[index]->c;
            }
        }

        const char* hash{nullptr};
        auto hashIdx = table.ColumnIndex(filesHashColumnName);
        if (hashIdx != std::numeric_limits<unsigned int>::max()) {
            auto row = table.GetRowByIndex(0, nullptr);
            if (row) {
                hash = row->cells[hashIdx]->c;
            }
        }

        // Add 1 to used and total slots because they are not counting room moderator
        roomsInfo.push_back(RoomInfo{room->lobbyRoomId, std::string(name), hostGuid,
                                     hostName ? hostName : "Unknown", password ? password : "",
                                     hash ? hash : "", totalSlots + 1, usedSlots + 1});
    }

    menuLobby->setRoomsInfo(std::move(roomsInfo));
}

CMenuCustomLobby::CConfirmBackMsgBoxButtonHandler::CConfirmBackMsgBoxButtonHandler(
    CMenuCustomLobby* menuLobby)
    : menuLobby{menuLobby}
{
    static game::CMidMsgBoxButtonHandlerVftable vftable = {
        (game::CMidMsgBoxButtonHandlerVftable::Destructor)destructor,
        (game::CMidMsgBoxButtonHandlerVftable::Handler)handler,
    };

    this->vftable = &vftable;
}

void __fastcall CMenuCustomLobby::CConfirmBackMsgBoxButtonHandler::destructor(
    CConfirmBackMsgBoxButtonHandler* thisptr,
    int /*%edx*/,
    char flags)
{ }

void __fastcall CMenuCustomLobby::CConfirmBackMsgBoxButtonHandler::handler(
    CConfirmBackMsgBoxButtonHandler* thisptr,
    int /*%edx*/,
    game::CMidgardMsgBox* msgBox,
    bool okPressed)
{
    using namespace game;

    auto interfManager = thisptr->menuLobby->interfaceData->interfManager.data;
    interfManager->CInterfManagerImpl::CInterfManager::vftable->hideInterface(interfManager,
                                                                              msgBox);
    if (msgBox) {
        msgBox->vftable->destructor(msgBox, 1);
    }

    if (okPressed) {
        auto menuPhase = thisptr->menuLobby->menuBaseData->menuPhase;
        getOriginalFunctions().menuPhaseBackToMainOrCloseGame(menuPhase, true);
    }
}

CMenuCustomLobby::CRoomPasswordInterf::CRoomPasswordInterf(CMenuCustomLobby* menu,
                                                           const char* dialogName)
    : CPopupDialogCustomBase{this, dialogName}
    , m_menu{menu}
{
    using namespace game;

    CPopupDialogInterfApi::get().constructor(this, dialogName, nullptr);

    assignButtonHandler("BTN_CANCEL", (CMenuBaseApi::Api::ButtonCallback)cancelBtnHandler);
    assignButtonHandler("BTN_OK", (CMenuBaseApi::Api::ButtonCallback)okBtnHandler);

    // Using EditFilter::Names for consistency with other game menus like CMenuNewSkirmishMulti
    setEditFilterAndLength("EDIT_PASSWORD", EditFilter::Names, 16, false);
}

void __fastcall CMenuCustomLobby::CRoomPasswordInterf::okBtnHandler(CRoomPasswordInterf* thisptr,
                                                                    int /*%edx*/)
{
    using namespace game;

    // TODO: generic getEditText for menus and dialogs
    const char* password = nullptr;
    auto passwordEdit = CDialogInterfApi::get().findEditBox(*thisptr->dialog, "EDIT_PASSWORD");
    if (passwordEdit) {
        password = passwordEdit->data->editBoxData.inputString.string;
    }

    // TODO: getSelectedRoom ? we need to stop refresh timer then or add joiningRoomPassword (or
    // copy whole room info?)
    auto menu = thisptr->m_menu;
    if (menu->getSelectedRoom()->password == password) {
        getNetService()->joinRoom(menu->joiningRoomId);
        menu->hideRoomPasswordDialog();
        menu->showWaitDialog();
    } else {
        auto msg{getInterfaceText(textIds().lobby.wrongRoomPassword.c_str())};
        if (msg.empty()) {
            msg = "Wrong room password";
        }
        showMessageBox(msg);
    }
}

void __fastcall CMenuCustomLobby::CRoomPasswordInterf::cancelBtnHandler(
    CRoomPasswordInterf* thisptr,
    int /*%edx*/)
{
    logDebug("lobby.log", "User canceled register account");
    thisptr->m_menu->hideRoomPasswordDialog();
}

} // namespace hooks
