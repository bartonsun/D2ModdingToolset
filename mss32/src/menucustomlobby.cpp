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
#include "interfaceutils.h"
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
    const auto& listBoxApi = CListBoxInterfApi::get();

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

    menuBaseApi.createMenu(this, dialogName);

    auto dialog = menuBaseApi.getDialogInterface(this);
    setButtonCallback(dialog, "BTN_BACK", backBtnHandler, this);
    setButtonCallback(dialog, "BTN_CREATE", createRoomBtnHandler, this);
    setButtonCallback(dialog, "BTN_LOAD", loadBtnHandler, this);
    setButtonCallback(dialog, "BTN_JOIN", joinRoomBtnHandler, this);

    auto listBoxRooms = CDialogInterfApi::get().findListBox(dialog, "LBOX_ROOMS");
    if (listBoxRooms) {
        SmartPointer functor;
        auto callback = (CMenuBaseApi::Api::ListBoxDisplayCallback)listBoxDisplayHandler;
        menuBaseApi.createListBoxDisplayFunctor(&functor, 0, this, &callback);
        listBoxApi.assignDisplaySurfaceFunctor(dialog, "LBOX_ROOMS", dialogName, &functor);
        listBoxApi.setElementsTotal(listBoxRooms, 0);
        SmartPointerApi::get().createOrFreeNoDtor(&functor, nullptr);
    }

    auto service = getNetService();
    fillNetMsgEntries();
    updateAccountText(service->getAccountName().c_str());

    service->addRoomsCallback(&roomsCallbacks);

    // Request rooms list as soon as possible, no need to wait for event
    service->searchRooms();
    createTimerEvent(&roomsListEvent, this, roomsListSearchHandler, roomListUpdateInterval);
}

CMenuCustomLobby ::~CMenuCustomLobby()
{
    using namespace game;

    hideRoomPasswordDialog();

    logDebug("transitions.log", "Delete rooms list event");
    UiEventApi::get().destructor(&roomsListEvent);

    auto service = getNetService();
    if (service) {
        service->removeRoomsCallback(&roomsCallbacks);
    }

    if (netMsgEntryData) {
        logDebug("transitions.log", "Delete custom lobby menu netMsgEntryData");
        NetMsgApi::get().freeEntryData(netMsgEntryData);
    }

    CMenuBaseApi::get().destructor(this);
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

void __fastcall CMenuCustomLobby::joinRoomBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    auto room = thisptr->getSelectedRoom();
    if (!room) {
        return;
    }

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
        // Store selected room info because the room list can be updated while the dialog is shown
        thisptr->joiningRoomId = room->id;
        thisptr->joiningRoomPassword = room->password;
        thisptr->showRoomPasswordDialog();
    }
}

void __fastcall CMenuCustomLobby::roomsListSearchHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    getNetService()->searchRooms();
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
        showMessageBox("No available races"); // TODO: localized message
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
    using namespace game;

    if (message->gameVersion != 40) {
        // "You are trying to join a game with a newer or an older version of the game."
        showMessageBox(getInterfaceText("X006ta0008"));
        return true;
    }

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

CMenuCustomLobby::RoomInfo CMenuCustomLobby::getRoomInfo(SLNet::RoomDescriptor* roomDescriptor)
{
    // Add 1 to used and total slots because they are not counting room moderator
    auto* moderator = getRoomModerator(roomDescriptor->roomMemberList);
    return {roomDescriptor->lobbyRoomId,
            roomDescriptor->GetProperty(DefaultRoomColumns::TC_ROOM_NAME)->c,
            moderator->guid,
            moderator->name,
            roomDescriptor->GetProperty(CNetCustomService::passwordColumnName)->c,
            roomDescriptor->GetProperty(CNetCustomService::filesHashColumnName)->c,
            (int)roomDescriptor->GetProperty(DefaultRoomColumns::TC_USED_PUBLIC_SLOTS)->i + 1,
            (int)roomDescriptor->GetProperty(DefaultRoomColumns::TC_TOTAL_PUBLIC_SLOTS)->i + 1};
}

SLNet::RoomMemberDescriptor* CMenuCustomLobby::getRoomModerator(
    DataStructures::List<SLNet::RoomMemberDescriptor>& roomMembers)
{
    for (unsigned int i = 0; i < roomMembers.Size(); ++i) {
        auto& member = roomMembers[i];
        if (member.roomMemberMode == RMM_MODERATOR) {
            return &member;
        }
    }

    return nullptr;
}

void CMenuCustomLobby::updateRooms(DataStructures::List<SLNet::RoomDescriptor*>& roomDescriptors)
{
    using namespace game;

    const auto& listBoxApi = CListBoxInterfApi::get();

    SLNet::RoomID selectedRoomId = (unsigned)-1;
    auto selectedRoom = getSelectedRoom();
    if (selectedRoom) {
        selectedRoomId = selectedRoom->id;
    }

    rooms.clear();
    rooms.reserve(roomDescriptors.Size());
    auto selectedIndex = (unsigned)-1;
    for (unsigned int i = 0; i < roomDescriptors.Size(); ++i) {
        rooms.push_back(getRoomInfo(roomDescriptors[i]));
        if (rooms.back().id == selectedRoomId) {
            selectedIndex = i;
        }
    }

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBox = CDialogInterfApi::get().findListBox(dialog, "LBOX_ROOMS");
    listBoxApi.setElementsTotal(listBox, (int)rooms.size());
    if (selectedIndex != (unsigned)-1) {
        listBoxApi.setSelectedIndex(listBox, selectedIndex);
    }
}

const CMenuCustomLobby::RoomInfo* CMenuCustomLobby::getSelectedRoom()
{
    using namespace game;

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBox = CDialogInterfApi::get().findListBox(dialog, "LBOX_ROOMS");
    auto index = (size_t)CListBoxInterfApi::get().selectedIndex(listBox);
    return index < rooms.size() ? &rooms[index] : nullptr;
}

void CMenuCustomLobby::updateAccountText(const char* accountName)
{
    using namespace game;

    const auto& textBoxApi = CTextBoxInterfApi::get();

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto textBox = CDialogInterfApi::get().findTextBox(dialog, "TXT_NICKNAME");
    if (!textBox) {
        return;
    }

    if (accountName) {
        textBoxApi.setString(textBox, fmt::format("\\fLarge;\\vC;\\hC;{:s}", accountName).c_str());
    } else {
        textBoxApi.setString(textBox, "");
    }
}

void CMenuCustomLobby::fillNetMsgEntries()
{
    using namespace game;

    const auto& netMsgApi = NetMsgApi::get();
    auto& netMsgMapEntryApi = CNetMsgMapEntryApi::get();

    netMsgApi.allocateEntryData(this->menuBaseData->menuPhase, &this->netMsgEntryData);

    auto infoCallback = (CNetMsgMapEntryApi::Api::MenusAnsInfoCallback)ansInfoMsgHandler;
    auto entry = netMsgMapEntryApi.allocateMenusAnsInfoEntry(this, infoCallback);
    netMsgApi.addEntry(this->netMsgEntryData, entry);

    auto versionCallback = (CNetMsgMapEntryApi::Api::GameVersionCallback)gameVersionMsgHandler;
    entry = netMsgMapEntryApi.allocateGameVersionEntry(this, versionCallback);
    netMsgApi.addEntry(this->netMsgEntryData, entry);
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
        showMessageBox("Created net client has net id 0"); // TODO: localized message
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
        showMessageBox("Could not request game version from server"); // TODO: localized message
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
        showMessageBox("Net service is null"); // TODO: localized message
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
        showMessageBox("Could not create net session"); // TODO: localized message
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

void CMenuCustomLobby::RoomListCallbacks::JoinByFilter_Callback(
    const SLNet::SystemAddress&,
    SLNet::JoinByFilter_Func* callResult)
{
    menuLobby->hideWaitDialog();

    switch (auto resultCode = callResult->resultCode) {
    case SLNet::REC_SUCCESS: {
        auto& room = callResult->joinedRoomResult.roomDescriptor;
        const char* roomName = room.GetProperty(DefaultRoomColumns::TC_ROOM_NAME)->c;
        logDebug("lobby.log", fmt::format("Joined a room {:s}", roomName));
        menuLobby->processJoin(roomName, getRoomModerator(room.roomMemberList)->guid);
        break;
    }

    case SLNet::REC_JOIN_BY_FILTER_NO_ROOMS: {
        auto msg{getInterfaceText(textIds().lobby.selectedRoomNoLongerExists.c_str())};
        if (msg.empty()) {
            msg = "The selected room no longer exists.";
        }
        showMessageBox(msg);
        break;
    }

    case SLNet::REC_JOIN_BY_FILTER_NO_SLOTS: {
        // Could not join game. Host did not report any available race.
        showMessageBox(getInterfaceText("X005TA0886"));
        break;
    }

    default: {
        auto error = SLNet::RoomsErrorCodeDescription::ToEnglish(callResult->resultCode);
        showMessageBox(error); // TODO: localized message
        break;
    }
    }
}

void CMenuCustomLobby::RoomListCallbacks::SearchByFilter_Callback(
    const SLNet::SystemAddress&,
    SLNet::SearchByFilter_Func* callResult)
{
    switch (auto resultCode = callResult->resultCode) {
    case SLNet::REC_SUCCESS: {
        menuLobby->updateRooms(callResult->roomsOutput);
        break;
    }

    default: {
        if (!getNetService()->loggedIn()) {
            // The error is expected, just silently remove the search event
            game::UiEventApi::get().destructor(&menuLobby->roomsListEvent);
            break;
        }

        showMessageBox(SLNet::RoomsErrorCodeDescription::ToEnglish(
            callResult->resultCode)); // TODO: localized message
        break;
    }
    }
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

    setButtonCallback(*dialog, "BTN_CANCEL", cancelBtnHandler, this);
    setButtonCallback(*dialog, "BTN_OK", okBtnHandler, this);

    // Using EditFilter::Names for consistency with other game menus like CMenuNewSkirmishMulti
    setEditBoxData(*dialog, "EDIT_PASSWORD", EditFilter::Names, 16, false);
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

    auto menu = thisptr->m_menu;
    if (menu->joiningRoomPassword == password) {
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
