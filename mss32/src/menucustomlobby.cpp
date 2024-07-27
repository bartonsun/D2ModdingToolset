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
#include "imageptrvector.h"
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
#include "pictureinterf.h"
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
    , m_peerCallback(this)
    , m_roomsCallback(this)
    , m_netMsgEntryData(nullptr)
    , m_roomPasswordDialog(nullptr)
    , m_chatMessageStock(chatMessageMaxStock)
    , m_userIcons{}
{
    using namespace game;

    const auto& menuBaseApi = CMenuBaseApi::get();
    const auto& dialogApi = CDialogInterfApi::get();
    const auto& listBoxApi = CListBoxInterfApi::get();
    const auto& editBoxApi = CEditBoxInterfApi::get();

    menuBaseApi.constructor(this, menuPhase);

    static RttiInfo<CMenuBaseVftable> rttiInfo = {};
    if (rttiInfo.locator == nullptr) {
        // Reuse object locator for our custom this.
        // We only need it for dynamic_cast<CAnimInterf>() to work properly without exceptions
        // when the game exits main loop and checks for current CInterface.
        // See DestroyAnimInterface() in IDA.
        replaceRttiInfo(rttiInfo, this->vftable);
        rttiInfo.vftable.destructor = (CInterfaceVftable::Destructor)&destructor;
        rttiInfo.vftable.handleKeyboard = (CInterfaceVftable::HandleKeyboard)&handleKeyboard;
    }
    this->vftable = &rttiInfo.vftable;

    menuBaseApi.createMenu(this, dialogName);

    auto dialog = menuBaseApi.getDialogInterface(this);
    setButtonCallback(dialog, "BTN_BACK", backBtnHandler, this);
    setButtonCallback(dialog, "BTN_CREATE", createBtnHandler, this);
    setButtonCallback(dialog, "BTN_LOAD", loadBtnHandler, this);
    setButtonCallback(dialog, "BTN_JOIN", joinBtnHandler, this);
    auto btnJoin = dialogApi.findButton(dialog, "BTN_JOIN");
    if (btnJoin) {
        btnJoin->vftable->setEnabled(btnJoin, false);
    }

    // Using generic findControl for optional controls to prevent error messages
    // TODO: findControl first, then findListBox to prevent possible crashes in case of control type
    // missmatch. Repeat for every occurrence.
    auto listBoxRooms = (CListBoxInterf*)dialogApi.findControl(dialog, "LBOX_ROOMS");
    if (listBoxRooms) {
        SmartPointer functor;
        auto callback = (CMenuBaseApi::Api::ListBoxDisplayCallback)listBoxRoomsDisplayHandler;
        menuBaseApi.createListBoxDisplayFunctor(&functor, 0, this, &callback);
        listBoxApi.assignDisplaySurfaceFunctor(dialog, "LBOX_ROOMS", dialogName, &functor);
        listBoxApi.setElementsTotal(listBoxRooms, 0);
        SmartPointerApi::get().createOrFreeNoDtor(&functor, nullptr);
    }

    ImagePtrVectorApi::get().reserve(&m_userIcons, 1);
    m_userIconsAreLeftOriented = true;
    auto listBoxUsers = (CListBoxInterf*)dialogApi.findControl(dialog, "LBOX_LPLAYERS");
    if (!listBoxUsers) {
        listBoxUsers = (CListBoxInterf*)dialogApi.findControl(dialog, "LBOX_RPLAYERS");
        m_userIconsAreLeftOriented = false;
    }
    if (listBoxUsers) {
        SmartPointer functor;
        auto callback = (CMenuBaseApi::Api::ListBoxDisplayCallback)listBoxUsersDisplayHandler;
        menuBaseApi.createListBoxDisplayFunctor(&functor, 0, this, &callback);
        listBoxApi.assignDisplaySurfaceFunctor(dialog,
                                               m_userIconsAreLeftOriented ? "LBOX_LPLAYERS"
                                                                          : "LBOX_RPLAYERS",
                                               dialogName, &functor);
        SmartPointerApi::get().createOrFreeNoDtor(&functor, nullptr);
    }

    auto listBoxChat = (CListBoxInterf*)dialogApi.findControl(dialog, "LBOX_CHAT");
    if (listBoxChat) {
        SmartPointer functor;
        using TextCallback = CMenuBaseApi::Api::ListBoxDisplayTextCallback;
        TextCallback callback{(TextCallback::Callback)listBoxChatDisplayHandler};
        menuBaseApi.createListBoxDisplayTextFunctor(&functor, 0, this, &callback);
        listBoxApi.assignDisplayTextFunctor(dialog, "LBOX_CHAT", dialogName, &functor, false);
        SmartPointerApi::get().createOrFreeNoDtor(&functor, nullptr);
    }

    auto editBoxChat = (CEditBoxInterf*)dialogApi.findControl(dialog, "EDIT_CHAT");
    if (editBoxChat) {
        editBoxApi.setInputLength(editBoxChat, chatMessageMaxLength);
    }

    auto service = getNetService();
    fillNetMsgEntries();

    bool playerFaceIsLeftOriented = true;
    auto imagePlayerFace = (CPictureInterf*)dialogApi.findControl(dialog, "IMG_LPLAYER_FACE");
    if (!imagePlayerFace) {
        playerFaceIsLeftOriented = false;
        imagePlayerFace = (CPictureInterf*)dialogApi.findControl(dialog, "IMG_RPLAYER_FACE");
    }
    if (imagePlayerFace) {
        auto* icon = getUserImage(service->getUserInfo(), playerFaceIsLeftOriented, true);
        CMqPoint offset{};
        CPictureInterfApi::get().setImage(imagePlayerFace, icon, &offset);
    }

    updateAccountText(service->getAccountName().c_str());

    service->addPeerCallback(&m_peerCallback);
    service->addRoomsCallback(&m_roomsCallback);

    // Request rooms list as soon as possible, no need to wait for event
    service->searchRooms();
    createTimerEvent(&m_roomsUpdateEvent, this, roomsUpdateEventCallback, roomsUpdateEventInterval);

    // Request users list as soon as possible, no need to wait for event
    service->queryOnlineUsers();
    createTimerEvent(&m_usersUpdateEvent, this, usersUpdateEventCallback, usersUpdateEventInterval);

    createTimerEvent(&m_chatMessageRegenEvent, this, chatMessageRegenEventCallback,
                     chatMessageRegenEventInterval);
}

CMenuCustomLobby ::~CMenuCustomLobby()
{
    using namespace game;

    const auto& uiEventApi = UiEventApi::get();

    hideRoomPasswordDialog();

    uiEventApi.destructor(&m_chatMessageRegenEvent);
    uiEventApi.destructor(&m_usersUpdateEvent);
    uiEventApi.destructor(&m_roomsUpdateEvent);

    ImagePtrVectorApi::get().destructor(&m_userIcons);

    auto service = getNetService();
    if (service) {
        service->removeRoomsCallback(&m_roomsCallback);
        service->removePeerCallback(&m_peerCallback);
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

// See CMenuLobbyHandleKeyboard (Akella 0x4e3a98)
int __fastcall CMenuCustomLobby::handleKeyboard(CMenuCustomLobby* thisptr,
                                                int /*%edx*/,
                                                int key,
                                                int a3)
{
    using namespace game;

    auto result = CMenuBaseApi::vftable()->handleKeyboard((CInterface*)thisptr, key, a3);
    if (key == VK_RETURN) {
        thisptr->sendChatMessage();
    }
    return result;
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
            message =
                "Unable to join the room because the owner's game version or files are different.";
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

void __fastcall CMenuCustomLobby::roomsUpdateEventCallback(CMenuCustomLobby* /*thisptr*/,
                                                           int /*%edx*/)
{
    getNetService()->searchRooms();
}

void __fastcall CMenuCustomLobby::usersUpdateEventCallback(CMenuCustomLobby* /*thisptr*/,
                                                           int /*%edx*/)
{
    getNetService()->queryOnlineUsers();
}

void __fastcall CMenuCustomLobby::chatMessageRegenEventCallback(CMenuCustomLobby* thisptr,
                                                                int /*%edx*/)
{
    if (thisptr->m_chatMessageStock < chatMessageMaxStock) {
        ++thisptr->m_chatMessageStock;
    }
}

void __fastcall CMenuCustomLobby::listBoxRoomsDisplayHandler(CMenuCustomLobby* thisptr,
                                                             int /*%edx*/,
                                                             game::ImagePointList* contents,
                                                             const game::CMqRect* lineArea,
                                                             int index,
                                                             bool selected)
{
    thisptr->updateListBoxRoomsRow(index, selected, lineArea, contents);
}

void __fastcall CMenuCustomLobby::listBoxUsersDisplayHandler(CMenuCustomLobby* thisptr,
                                                             int /*%edx*/,
                                                             game::ImagePointList* contents,
                                                             const game::CMqRect* lineArea,
                                                             int index,
                                                             bool selected)
{
    thisptr->updateListBoxUsersRow(index, selected, lineArea, contents);
}

void __fastcall CMenuCustomLobby::listBoxChatDisplayHandler(CMenuCustomLobby* thisptr,
                                                            int /*%edx*/,
                                                            game::String* string,
                                                            bool,
                                                            int selectedIndex)
{
    const auto& messages = thisptr->m_chatMessages;
    if (selectedIndex < 0 || selectedIndex >= (int)messages.size()) {
        return;
    }

    auto messageText{getInterfaceText(textIds().lobby.chatMessage.c_str())};
    if (messageText.empty()) {
        messageText = "%SENDER%: %MSG%";
    }

    const auto& message = messages[selectedIndex];
    replace(messageText, "%SENDER%", message.sender);
    replace(messageText, "%MSG%", message.text);
    game::StringApi::get().initFromString(string, messageText.c_str());
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
            getRoomModerator(roomDescriptor->roomMemberList)->name,
            roomDescriptor->GetProperty(CNetCustomService::gameNameColumnName)->c,
            roomDescriptor->GetProperty(CNetCustomService::passwordColumnName)->c,
            roomDescriptor->GetProperty(CNetCustomService::gameFilesHashColumnName)->c,
            roomDescriptor->GetProperty(CNetCustomService::gameVersionColumnName)->c,
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

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& listBoxApi = CListBoxInterfApi::get();

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBox = dialogApi.findListBox(dialog, "LBOX_ROOMS");
    if (!listBox->vftable->isOnTop(listBox)) {
        // If the listBox is not on top then the game will only remove its childs without rendering
        // the new ones. This happens when any popup dialog is displayed.
        return;
    }

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

    listBoxApi.setElementsTotal(listBox, (int)m_rooms.size());
    if (selectedIndex != (unsigned)-1) {
        listBoxApi.setSelectedIndex(listBox, selectedIndex);
    }

    auto btnJoin = dialogApi.findButton(dialog, "BTN_JOIN");
    if (btnJoin) {
        btnJoin->vftable->setEnabled(btnJoin, !m_rooms.empty());
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

void CMenuCustomLobby::updateListBoxRoomsRow(int rowIndex,
                                             bool selected,
                                             const game::CMqRect* lineArea,
                                             game::ImagePointList* contents)
{
    if ((size_t)rowIndex >= m_rooms.size()) {
        return;
    }
    const auto& room = m_rooms[(size_t)rowIndex];

    addListBoxRoomsCellText("TXT_HOST", fmt::format("\\vC;{:s}", room.hostName).c_str(), lineArea,
                            contents);
    addListBoxRoomsCellText("TXT_DESCRIPTION", fmt::format("\\vC;{:s}", room.gameName).c_str(),
                            lineArea, contents);
    addListBoxRoomsCellText("TXT_VERSION", fmt::format("\\vC;{:s}", room.gameVersion).c_str(),
                            lineArea, contents);
    addListBoxRoomsCellText(
        "TXT_PLAYERS", fmt::format("\\vC;\\hC;{:d}/{:d}", room.usedSlots, room.totalSlots).c_str(),
        lineArea, contents);

    if (!room.password.empty()) {
        addListBoxRoomsCellImage("IMG_PROTECTED", "ROOM_PROTECTED", lineArea, contents);
    }

    if (selected) {
        addListBoxRoomsSelectionOutline(lineArea, contents);
    }
}

void CMenuCustomLobby::addListBoxRoomsCellText(const char* columnName,
                                               const char* value,
                                               const game::CMqRect* lineArea,
                                               game::ImagePointList* contents)
{
    using namespace game;

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& createFreePtr = SmartPointerApi::get().createOrFree;
    const auto& image2TextApi = CImage2TextApi::get();

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto columnHeader = dialogApi.findTextBox(dialog, columnName);
    if (!columnHeader) {
        return;
    }

    auto columnHeaderRect = columnHeader->vftable->getArea(columnHeader);
    auto cellWidth = columnHeaderRect->right - columnHeaderRect->left;
    auto cellHeight = lineArea->bottom - lineArea->top;

    auto image2Text = (CImage2Text*)Memory::get().allocate(sizeof(CImage2Text));
    image2TextApi.constructor(image2Text, cellWidth, cellHeight);
    image2TextApi.setText(image2Text, value);

    auto listBox = dialogApi.findListBox(dialog, "LBOX_ROOMS");
    auto listBoxRect = listBox->vftable->getArea(listBox);

    ImagePtrPointPair pair{};
    createFreePtr((SmartPointer*)&pair.first, image2Text);
    pair.second.x = columnHeaderRect->left - listBoxRect->left;
    pair.second.y = 0;
    ImagePointListApi::get().add(contents, &pair);
    createFreePtr((SmartPointer*)&pair.first, nullptr);
}

void CMenuCustomLobby::addListBoxRoomsCellImage(const char* columnName,
                                                const char* imageName,
                                                const game::CMqRect* lineArea,
                                                game::ImagePointList* contents)
{
    using namespace game;

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& createFreePtr = SmartPointerApi::get().createOrFree;

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto columnHeader = dialogApi.findPicture(dialog, columnName);
    if (!columnHeader) {
        return;
    }

    auto columnHeaderRect = columnHeader->vftable->getArea(columnHeader);
    auto image = AutoDialogApi::get().loadImage(imageName);

    // Calculate offset to center the image
    CMqPoint imageSize{};
    image->vftable->getSize(image, &imageSize);
    CMqPoint imageOffset{(columnHeaderRect->right - columnHeaderRect->left - imageSize.x) / 2,
                         (lineArea->bottom - lineArea->top - imageSize.y) / 2};

    auto listBox = dialogApi.findListBox(dialog, "LBOX_ROOMS");
    auto listBoxRect = listBox->vftable->getArea(listBox);

    ImagePtrPointPair pair{};
    createFreePtr((SmartPointer*)&pair.first, image);
    pair.second.x = imageOffset.x + columnHeaderRect->left - listBoxRect->left;
    pair.second.y = imageOffset.y;
    ImagePointListApi::get().add(contents, &pair);
    createFreePtr((SmartPointer*)&pair.first, nullptr);
}

void CMenuCustomLobby::addListBoxRoomsSelectionOutline(const game::CMqRect* lineArea,
                                                       game::ImagePointList* contents)
{
    using namespace game;

    const auto& createFreePtr = SmartPointerApi::get().createOrFree;

    // 0x0 - transparent, 0xff - opaque
    game::Color color{};
    CMqPoint size{lineArea->right - lineArea->left, lineArea->bottom - lineArea->top};
    auto outline = (CImage2Outline*)Memory::get().allocate(sizeof(CImage2Outline));
    CImage2OutlineApi::get().constructor(outline, &size, &color, 0xff);

    ImagePtrPointPair pair{};
    createFreePtr((SmartPointer*)&pair.first, outline);
    pair.second.x = 0;
    pair.second.y = 0;
    ImagePointListApi::get().add(contents, &pair);
    createFreePtr((SmartPointer*)&pair.first, nullptr);
}

void CMenuCustomLobby::updateListBoxUsersRow(int rowIndex,
                                             bool /*selected*/,
                                             const game::CMqRect* lineArea,
                                             game::ImagePointList* contents)
{
    using namespace game;

    if (rowIndex < 0 || rowIndex >= (int)m_users.size()) {
        return;
    }
    const auto& user = m_users[rowIndex];

    if (rowIndex < 0 || rowIndex >= (int)m_userIcons.size()) {
        logDebug(
            "lobby.log",
            fmt::format(
                "Failed to update users listbox because the icon at index {:d} is out of list bounds",
                rowIndex));
        return;
    }
    const auto& icon = m_userIcons.bgn[rowIndex];

    auto playerName{getInterfaceText(textIds().lobby.playerNameInList.c_str())};
    if (playerName.empty()) {
        playerName = "\\vC;\\hC;\\fSmall;%NAME%";
    }
    replace(playerName, "%NAME%", user.name.C_String());

    CMqPoint iconSize;
    icon.data->vftable->getSize(icon.data, &iconSize);

    // Place text area under the icon
    CMqRect textClientArea = {0, iconSize.y, lineArea->right - lineArea->left,
                              lineArea->bottom - lineArea->top};
    // Center image horizontally
    CMqPoint iconClientPos{(lineArea->right - lineArea->left - iconSize.x) / 2, 0};
    ImagePointListApi::get().addImageWithText(contents, lineArea, &icon, &iconClientPos,
                                              playerName.c_str(), &textClientArea, false, 0);
}

game::IMqImage2* CMenuCustomLobby::getUserImage(const CNetCustomService::UserInfo& user,
                                                bool left,
                                                bool big)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& idApi = CMidgardIDApi::get();

    CMidgardID unitImplId{};
    auto token = user.guid.g;
    // A little bit of easter eggs is never wrong
    if (user.name == "UnveN") {
        idApi.fromString(&unitImplId, "G000UU0101"); /* Skeleton */
    } else if (token % 23 == 0) {
        idApi.fromString(&unitImplId, "G000UU5031"); /* Master Thug */
    } else if (token % 69 == 0) {
        idApi.fromString(&unitImplId, "G000UU6121"); /* Drega Zul */
    } else if (token % 81 == 0) {
        idApi.fromString(&unitImplId, "G000UU8046"); /* Shamaness */
    } else {
        const size_t ID_COUNT = 18;
        const char* unitImplIds[ID_COUNT] =
            {"G000UU0001" /* Squire */,     "G000UU0006" /* Archer */,
             "G000UU0008" /* Apprentice */, "G000UU0011" /* Acolyte */,
             "G000UU0036" /* Dwarf */,      "G000UU0026" /* Axe Thrower */,
             "G000UU0033" /* Tenderfoot */, "G000UU0052" /* Possessed */,
             "G000UU0062" /* Cultist */,    "G000UU0086" /* Fighter */,
             "G000UU0080" /* Initiate */,   "G000UU0078" /* Ghost */,
             "G000UU0092" /* Werewolf */,   "G000UU8014" /* Centaur Lancer */,
             "G000UU8018" /* Scout */,      "G000UU8025" /* Adept */,
             "G000UU8031" /* Spiritess */,  "G000UU8036" /* Ent Minor */};
        idApi.fromString(&unitImplId, unitImplIds[token % ID_COUNT]);
    }

    auto faceImage = fn.createUnitFaceImage(&unitImplId, big);
    faceImage->vftable->setPercentHp(faceImage, 100);
    faceImage->vftable->setUnknown68(faceImage, 0);
    faceImage->vftable->setLeftSide(faceImage, left);

    return (IMqImage2*)faceImage;
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
    midgardApi.createNetClient(midgard, service->getAccountName().c_str(), true);
    midgardApi.setClientsNetProxy(midgard, menuPhase);

    CMenusReqVersionMsg reqVersionMsg;
    reqVersionMsg.vftable = NetMessagesApi::getMenusReqVersionVftable();
    if (!midgardApi.sendNetMsgToServer(midgard, &reqVersionMsg)) {
        // Cannot connect to game session
        midgardApi.clearNetworkState(midgard);
        showMessageBox(getInterfaceText("X003TA0001"));
    }
}

void CMenuCustomLobby::addChatMessage(const char* sender, const char* message)
{
    using namespace game;

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& listBoxApi = CListBoxInterfApi::get();

    if (m_chatMessages.size() >= chatMessageMaxCount) {
        m_chatMessages.pop_front();
    }
    m_chatMessages.push_back({sender, message});

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBoxChat = (CListBoxInterf*)dialogApi.findControl(dialog, "LBOX_CHAT");
    if (!listBoxChat || !listBoxChat->vftable->isOnTop(listBoxChat)) {
        // If the listBox is not on top then the game will only remove its childs without rendering
        // the new ones. This happens when any popup dialog is displayed.
        return;
    }

    listBoxApi.setElementsTotal(listBoxChat, (int)m_chatMessages.size());
}

void CMenuCustomLobby::sendChatMessage()
{
    using namespace game;

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& editBoxApi = CEditBoxInterfApi::get();

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto editBoxChat = (CEditBoxInterf*)dialogApi.findControl(dialog, "EDIT_CHAT");
    if (!editBoxChat) {
        return;
    }

    auto message = getEditBoxText(dialog, "EDIT_CHAT");
    if (!strlen(message)) {
        return;
    }

    if (m_chatMessageStock == 0) {
        auto msg{getInterfaceText(textIds().lobby.tooManyChatMessages.c_str())};
        if (msg.empty()) {
            msg = "Too many chat messages. Wait a couple of seconds.";
        }
        showMessageBox(msg);
        return;
    }

    getNetService()->sendChatMessage(message);
    editBoxApi.setString(editBoxChat, "");
    --m_chatMessageStock;
}

void CMenuCustomLobby::updateUsers(std::vector<CNetCustomService::UserInfo> users)
{
    using namespace game;

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& listBoxApi = CListBoxInterfApi::get();
    const auto& smartPtrApi = SmartPointerApi::get();
    const auto& imagePtrVectorApi = ImagePtrVectorApi::get();

    auto dialog = CMenuBaseApi::get().getDialogInterface(this);
    auto listBox = dialogApi.findListBox(dialog, m_userIconsAreLeftOriented ? "LBOX_LPLAYERS"
                                                                            : "LBOX_RPLAYERS");
    if (!listBox->vftable->isOnTop(listBox)) {
        // If the listBox is not on top then the game will only remove its childs without rendering
        // the new ones. This happens when any popup dialog is displayed.
        return;
    }

    // See CManageStkInterfSetLeaderAbilities (Akella 0x497E5A)
    imagePtrVectorApi.destructor(&m_userIcons);
    imagePtrVectorApi.reserve(&m_userIcons, 1);
    for (const auto& user : users) {
        auto* icon = getUserImage(user, m_userIconsAreLeftOriented, false);
        ImagePtr iconPtr{};
        smartPtrApi.createOrFree((SmartPointer*)&iconPtr, icon);
        imagePtrVectorApi.pushBack(&m_userIcons, &iconPtr);
        smartPtrApi.createOrFree((SmartPointer*)&iconPtr, nullptr);
    }

    m_users = std::move(users);
    listBoxApi.setElementsTotal(listBox, (int)m_users.size());
}

void CMenuCustomLobby::PeerCallback::onPacketReceived(DefaultMessageIDTypes type,
                                                      SLNet::RakPeerInterface* peer,
                                                      const SLNet::Packet* packet)
{
    switch (type) {
    case ID_LOBBY_CHAT_MESSAGE: {
        SLNet::RakString sender;
        SLNet::RakString text;
        getNetService()->readChatMessage(packet, sender, text);
        m_menu->addChatMessage(sender.C_String(), text.C_String());
        break;
    }

    case ID_LOBBY_GET_ONLINE_USERS_RESPONSE: {
        m_menu->updateUsers(getNetService()->readOnlineUsers(packet));
        break;
    }
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
            game::UiEventApi::get().destructor(&m_menu->m_roomsUpdateEvent);
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

CMenuCustomLobby::CRoomPasswordInterf::CRoomPasswordInterf(CMenuCustomLobby* menu)
    : CPopupDialogCustomBase{this, roomPasswordDialogName}
    , m_menu{menu}
{
    using namespace game;

    CPopupDialogInterfApi::get().constructor(this, roomPasswordDialogName, nullptr);

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
