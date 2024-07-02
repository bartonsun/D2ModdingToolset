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
#include "restrictions.h"
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
    , m_roomsCallback(this)
    , m_netMsgEntryData(nullptr)
    , m_roomPasswordDialog(nullptr)
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
    setButtonCallback(dialog, "BTN_CREATE", createBtnHandler, this);
    setButtonCallback(dialog, "BTN_LOAD", loadBtnHandler, this);
    setButtonCallback(dialog, "BTN_JOIN", joinBtnHandler, this);

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

    service->addRoomsCallback(&m_roomsCallback);

    // Request rooms list as soon as possible, no need to wait for event
    service->searchRooms();
    createTimerEvent(&m_roomsListEvent, this, roomsListSearchHandler, roomListUpdateInterval);
}

CMenuCustomLobby ::~CMenuCustomLobby()
{
    using namespace game;

    hideRoomPasswordDialog();

    logDebug("transitions.log", "Delete rooms list event");
    UiEventApi::get().destructor(&m_roomsListEvent);

    auto service = getNetService();
    if (service) {
        service->removeRoomsCallback(&m_roomsCallback);
    }

    if (m_netMsgEntryData) {
        logDebug("transitions.log", "Delete custom lobby menu netMsgEntryData");
        NetMsgApi::get().freeEntryData(m_netMsgEntryData);
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

    if (m_roomPasswordDialog) {
        logDebug("lobby.log", "Error showing room-password dialog that is already shown");
        return;
    }

    m_roomPasswordDialog = (CRoomPasswordInterf*)Memory::get().allocate(
        sizeof(CRoomPasswordInterf));
    showInterface(new (m_roomPasswordDialog) CRoomPasswordInterf(this));
}

void CMenuCustomLobby::hideRoomPasswordDialog()
{
    if (m_roomPasswordDialog) {
        hideInterface(m_roomPasswordDialog);
        m_roomPasswordDialog->vftable->destructor(m_roomPasswordDialog, 1);
        m_roomPasswordDialog = nullptr;
    }
}

void __fastcall CMenuCustomLobby::createBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    using namespace game;

    auto menuPhase = thisptr->menuBaseData->menuPhase;
    menuPhase->data->host = true;
    CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::CustomLobby2NewSkirmish);
}

void __fastcall CMenuCustomLobby::loadBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
{
    using namespace game;

    auto menuPhase = thisptr->menuBaseData->menuPhase;
    menuPhase->data->host = true;
    CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::CustomLobby2LoadSkirmish);
}

void __fastcall CMenuCustomLobby::joinBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/)
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
        thisptr->m_joiningRoomId = room->id;
        thisptr->m_joiningRoomPassword = room->password;
        thisptr->showRoomPasswordDialog();
    }
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
    if (thisptr->m_rooms.empty()) {
        return;
    }

    using namespace game;

    auto& createFreePtr = SmartPointerApi::get().createOrFree;
    auto& imageApi = CImage2TextApi::get();

    const auto width = lineArea->right - lineArea->left;
    const auto height = lineArea->bottom - lineArea->top;
    const auto& room = thisptr->m_rooms[(size_t)index % thisptr->m_rooms.size()];

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

bool __fastcall CMenuCustomLobby::gameVersionMsgHandler(CMenuCustomLobby* menu,
                                                        int /*%edx*/,
                                                        const game::CGameVersionMsg* message,
                                                        std::uint32_t idFrom)
{
    using namespace game;

    const auto& midgardApi = CMidgardApi::get();

    logDebug("lobby.log", fmt::format("CGameVersionMsg received from 0x{:x}", idFrom));

    if (message->gameVersion != GameVersion::RiseOfTheElves) {
        // You are trying to join a game with a newer or an older version of the game.
        midgardApi.clearNetworkState(midgardApi.instance());
        showMessageBox(getInterfaceText("X006TA0008"));
        return true;
    }

    CMenusReqInfoMsg reqInfoMsg;
    reqInfoMsg.vftable = NetMessagesApi::getMenusReqInfoVftable();
    return midgardApi.sendNetMsgToServer(midgardApi.instance(), &reqInfoMsg);
}

bool __fastcall CMenuCustomLobby::ansInfoMsgHandler(CMenuCustomLobby* menu,
                                                    int /*%edx*/,
                                                    const game::CMenusAnsInfoMsg* message,
                                                    std::uint32_t idFrom)
{
    using namespace game;

    const auto& midgardApi = CMidgardApi::get();
    const auto& listApi = RaceCategoryListApi::get();

    logDebug("lobby.log", fmt::format("CMenusAnsInfoMsg received from 0x{:x}", idFrom));

    menu->hideWaitDialog();

    if (message->raceIds.length == 0) {
        // Could not join game.  Host did not report any available race.
        midgardApi.clearNetworkState(midgardApi.instance());
        showMessageBox(getInterfaceText("X005TA0886"));
        return true;
    }

    auto globalData = *GlobalDataApi::get().getGlobalData();
    auto racesTable = globalData->raceCategories;
    auto& findRaceById = LRaceCategoryTableApi::get().findCategoryById;

    auto menuPhase = menu->menuBaseData->menuPhase;
    auto phaseData = menuPhase->data;

    auto races = &phaseData->races;
    listApi.freeNodes(races);
    for (auto node = message->raceIds.head->next; node != message->raceIds.head;
         node = node->next) {
        const int raceId = static_cast<const int>(node->data);

        LRaceCategory category{};
        findRaceById(racesTable, &category, &raceId);

        listApi.add(races, &category);
    }

    phaseData->host = false;
    phaseData->unknown8 = false;
    phaseData->suggestedLevel = message->suggestedLevel;
    allocateString(&phaseData->scenarioName, message->scenarioName);
    allocateString(&phaseData->scenarioDescription, message->scenarioDescription);

    game::CMenuPhaseApi::get().switchPhase(menuPhase, MenuTransition::CustomLobby2LobbyJoin);
    return true;
}

CMenuCustomLobby::RoomInfo CMenuCustomLobby::getRoomInfo(SLNet::RoomDescriptor* roomDescriptor)
{
    // Add 1 to used and total slots because they are not counting room moderator
    return {roomDescriptor->lobbyRoomId,
            roomDescriptor->GetProperty(DefaultRoomColumns::TC_ROOM_NAME)->c,
            getRoomModerator(roomDescriptor->roomMemberList)->name,
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

    m_rooms.clear();
    m_rooms.reserve(roomDescriptors.Size());
    auto selectedIndex = (unsigned)-1;
    for (unsigned int i = 0; i < roomDescriptors.Size(); ++i) {
        m_rooms.push_back(getRoomInfo(roomDescriptors[i]));
        if (m_rooms.back().id == selectedRoomId) {
            selectedIndex = i;
        }
    }

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBox = CDialogInterfApi::get().findListBox(dialog, "LBOX_ROOMS");
    listBoxApi.setElementsTotal(listBox, (int)m_rooms.size());
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
    return index < m_rooms.size() ? &m_rooms[index] : nullptr;
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

    textBoxApi.setString(textBox, accountName ? accountName : "");
}

void CMenuCustomLobby::fillNetMsgEntries()
{
    using namespace game;

    const auto& netMsgApi = NetMsgApi::get();
    auto& netMsgMapEntryApi = CNetMsgMapEntryApi::get();

    netMsgApi.allocateEntryData(this->menuBaseData->menuPhase, &this->m_netMsgEntryData);

    auto infoCallback = (CNetMsgMapEntryApi::Api::MenusAnsInfoCallback)ansInfoMsgHandler;
    auto entry = netMsgMapEntryApi.allocateMenusAnsInfoEntry(this, infoCallback);
    netMsgApi.addEntry(this->m_netMsgEntryData, entry);

    auto versionCallback = (CNetMsgMapEntryApi::Api::GameVersionCallback)gameVersionMsgHandler;
    entry = netMsgMapEntryApi.allocateGameVersionEntry(this, versionCallback);
    netMsgApi.addEntry(this->m_netMsgEntryData, entry);
}

// See CMenuSession::JoinGameBtnCallback (Akella 0x4f0136)
void CMenuCustomLobby::joinServer(SLNet::RoomDescriptor* roomDescriptor)
{
    using namespace game;

    const auto& midgardApi = CMidgardApi::get();

    auto service = getNetService();
    CNetCustomSession* session = nullptr;
    CNetCustomSessEnum sessEnum{getRoomModerator(roomDescriptor->roomMemberList)->guid,
                                roomDescriptor->GetProperty(DefaultRoomColumns::TC_ROOM_NAME)->c};
    service->vftable->joinSession(service, (IMqNetSession**)&session, (IMqNetSessEnum*)&sessEnum,
                                  nullptr);

    // Add 1 to total slots because they are not counting room moderator
    auto maxPlayers = (int)roomDescriptor->GetProperty(DefaultRoomColumns::TC_TOTAL_PUBLIC_SLOTS)->i
                      + 1;
    session->setMaxPlayers(maxPlayers);

    auto menuPhase = menuBaseData->menuPhase;
    menuPhase->data->maxPlayers = maxPlayers;

    auto midgard = midgardApi.instance();
    auto& currentSession = midgard->data->netSession;
    if (currentSession) {
        currentSession->vftable->destructor(currentSession, 1);
    }
    currentSession = session;
    midgard->data->host = false;
    midgardApi.createNetClient(midgard, service->getAccountName().c_str(), false);
    midgardApi.setClientsNetProxy(midgard, menuPhase);

    CMenusReqVersionMsg reqVersionMsg;
    reqVersionMsg.vftable = NetMessagesApi::getMenusReqVersionVftable();
    if (!midgardApi.sendNetMsgToServer(midgard, &reqVersionMsg)) {
        // Cannot connect to game session
        midgardApi.clearNetworkState(midgard);
        showMessageBox(getInterfaceText("X003TA0001"));
    }
}

void CMenuCustomLobby::RoomsCallback::JoinByFilter_Callback(const SLNet::SystemAddress&,
                                                            SLNet::JoinByFilter_Func* callResult)
{
    m_menu->hideWaitDialog();

    switch (auto resultCode = callResult->resultCode) {
    case SLNet::REC_SUCCESS: {
        m_menu->joinServer(&callResult->joinedRoomResult.roomDescriptor);
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
        auto msg{getInterfaceText(textIds().lobby.joinRoomFailed.c_str())};
        if (msg.empty()) {
            msg = "Could not join a room.\n%ERROR%";
        }
        replace(msg, "%ERROR%", SLNet::RoomsErrorCodeDescription::ToEnglish(resultCode));
        showMessageBox(msg);
        break;
    }
    }
}

void CMenuCustomLobby::RoomsCallback::SearchByFilter_Callback(
    const SLNet::SystemAddress&,
    SLNet::SearchByFilter_Func* callResult)
{
    switch (auto resultCode = callResult->resultCode) {
    case SLNet::REC_SUCCESS: {
        m_menu->updateRooms(callResult->roomsOutput);
        break;
    }

    default: {
        if (!getNetService()->loggedIn()) {
            // The error is expected, just silently remove the search event
            game::UiEventApi::get().destructor(&m_menu->m_roomsListEvent);
            break;
        }

        auto msg{getInterfaceText(textIds().lobby.searchRoomsFailed.c_str())};
        if (msg.empty()) {
            msg = "Could not search for rooms.\n%ERROR%";
        }
        replace(msg, "%ERROR%", SLNet::RoomsErrorCodeDescription::ToEnglish(resultCode));
        showMessageBox(msg);
        break;
    }
    }
}

CMenuCustomLobby::CConfirmBackMsgBoxButtonHandler::CConfirmBackMsgBoxButtonHandler(
    CMenuCustomLobby* menu)
    : m_menu{menu}
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

    auto interfManager = thisptr->m_menu->interfaceData->interfManager.data;
    interfManager->CInterfManagerImpl::CInterfManager::vftable->hideInterface(interfManager,
                                                                              msgBox);
    if (msgBox) {
        msgBox->vftable->destructor(msgBox, 1);
    }

    if (okPressed) {
        auto menuPhase = thisptr->m_menu->menuBaseData->menuPhase;
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
    setEditBoxData(*dialog, "EDIT_PASSWORD", EditFilter::Names,
                   gameRestrictions().passwordMaxLength, false);
}

void __fastcall CMenuCustomLobby::CRoomPasswordInterf::okBtnHandler(CRoomPasswordInterf* thisptr,
                                                                    int /*%edx*/)
{
    auto menu = thisptr->m_menu;
    if (menu->m_joiningRoomPassword == getEditBoxText(*thisptr->dialog, "EDIT_PASSWORD")) {
        getNetService()->joinRoom(menu->m_joiningRoomId);
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
