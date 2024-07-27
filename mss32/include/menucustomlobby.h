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

#ifndef MENUCUSTOMLOBBY_H
#define MENUCUSTOMLOBBY_H

#include "d2vector.h"
#include "menubase.h"
#include "menucustombase.h"
#include "midmsgboxbuttonhandler.h"
#include "popupdialoginterf.h"
#include "uievent.h"
#include <DS_List.h>
#include <Lobby2Message.h>
#include <RoomsPlugin.h>
#include <deque>
#include <string>
#include <vector>

namespace game {
struct NetMsgEntryData;
struct CMenusAnsInfoMsg;
struct CGameVersionMsg;
} // namespace game

namespace hooks {

class CMenuCustomLobby
    : public game::CMenuBase
    , public CMenuCustomBase
{
public:
    static constexpr char dialogName[] = "DLG_CUSTOM_LOBBY";
    static constexpr char roomPasswordDialogName[] = "DLG_ROOM_PASSWORD";
    static constexpr char transitionFromProtoName[] = "TRANS_PROTO2CUSTOMLOBBY";
    static constexpr char transitionFromBlackName[] = "TRANS_BLACK2CUSTOMLOBBY";
    static constexpr std::uint32_t roomsUpdateEventInterval{5000};
    static constexpr std::uint32_t usersUpdateEventInterval{5000};
    static constexpr std::uint32_t chatMessageMaxLength{50};
    static constexpr std::uint32_t chatMessageMaxCount{50};
    static constexpr std::uint32_t chatMessageMaxStock{3};
    static constexpr std::uint32_t chatMessageRegenEventInterval{3000};

    CMenuCustomLobby(game::CMenuPhase* menuPhase);
    ~CMenuCustomLobby();

protected:
    // CInterface
    static void __fastcall destructor(CMenuCustomLobby* thisptr, int /*%edx*/, char flags);
    static int __fastcall handleKeyboard(CMenuCustomLobby* thisptr, int /*%edx*/, int key, int a3);

    struct RoomInfo;

    void showRoomPasswordDialog();
    void hideRoomPasswordDialog();
    void updateRooms(DataStructures::List<SLNet::RoomDescriptor*>& roomDescriptors);
    const RoomInfo* getSelectedRoom();
    void updateAccountText(const char* accountName);
    void updateListBoxRoomsRow(int rowIndex,
                               bool selected,
                               const game::CMqRect* lineArea,
                               game::ImagePointList* contents);
    void addListBoxRoomsCellText(const char* columnName,
                                 const char* value,
                                 const game::CMqRect* lineArea,
                                 game::ImagePointList* contents);
    void addListBoxRoomsCellImage(const char* columnName,
                                  const char* imageName,
                                  const game::CMqRect* lineArea,
                                  game::ImagePointList* contents);
    void addListBoxRoomsSelectionOutline(const game::CMqRect* lineArea,
                                         game::ImagePointList* contents);
    void updateListBoxUsersRow(int rowIndex,
                               bool selected,
                               const game::CMqRect* lineArea,
                               game::ImagePointList* contents);
    game::IMqImage2* getUserImage(const CNetCustomService::UserInfo& user, bool left, bool big);
    void fillNetMsgEntries();
    void joinServer(SLNet::RoomDescriptor* roomDescriptor);
    void addChatMessage(const char* sender, const char* message);
    void sendChatMessage();
    void updateUsers(std::vector<CNetCustomService::UserInfo> users);

    static RoomInfo getRoomInfo(SLNet::RoomDescriptor* roomDescriptor);
    static SLNet::RoomMemberDescriptor* getRoomModerator(
        DataStructures::List<SLNet::RoomMemberDescriptor>& roomMembers);

    static void __fastcall createBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall loadBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall joinBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall backBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall roomsUpdateEventCallback(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall usersUpdateEventCallback(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall chatMessageRegenEventCallback(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall listBoxRoomsDisplayHandler(CMenuCustomLobby* thisptr,
                                                      int /*%edx*/,
                                                      game::ImagePointList* contents,
                                                      const game::CMqRect* lineArea,
                                                      int index,
                                                      bool selected);
    static void __fastcall listBoxUsersDisplayHandler(CMenuCustomLobby* thisptr,
                                                      int /*%edx*/,
                                                      game::ImagePointList* contents,
                                                      const game::CMqRect* lineArea,
                                                      int index,
                                                      bool selected);
    static void __fastcall listBoxChatDisplayHandler(CMenuCustomLobby* thisptr,
                                                     int /*%edx*/,
                                                     game::String* string,
                                                     bool,
                                                     int selectedIndex);
    static bool __fastcall gameVersionMsgHandler(CMenuCustomLobby* menu,
                                                 int /*%edx*/,
                                                 const game::CGameVersionMsg* message,
                                                 std::uint32_t idFrom);
    static bool __fastcall ansInfoMsgHandler(CMenuCustomLobby* menu,
                                             int /*%edx*/,
                                             const game::CMenusAnsInfoMsg* message,
                                             std::uint32_t idFrom);

    struct RoomInfo
    {
        SLNet::RoomID id;
        std::string hostName;
        std::string gameName;
        std::string password;
        std::string gameFilesHash;
        std::string gameVersion;
        int usedSlots;
        int totalSlots;
    };

    struct ChatMessage
    {
        std::string sender;
        std::string text;
    };

    class PeerCallback : public NetPeerCallback
    {
    public:
        PeerCallback(CMenuCustomLobby* menu)
            : m_menu{menu}
        { }

        ~PeerCallback() override = default;

        void onPacketReceived(DefaultMessageIDTypes type,
                              SLNet::RakPeerInterface* peer,
                              const SLNet::Packet* packet) override;

    private:
        CMenuCustomLobby* m_menu;
    };

    class RoomsCallback : public SLNet::RoomsCallback
    {
    public:
        RoomsCallback(CMenuCustomLobby* menu)
            : m_menu{menu}
        { }

        ~RoomsCallback() override = default;

        void JoinByFilter_Callback(const SLNet::SystemAddress& senderAddress,
                                   SLNet::JoinByFilter_Func* callResult) override;

        void SearchByFilter_Callback(const SLNet::SystemAddress& senderAddress,
                                     SLNet::SearchByFilter_Func* callResult) override;

    private:
        CMenuCustomLobby* m_menu;
    };

    struct CConfirmBackMsgBoxButtonHandler : public game::CMidMsgBoxButtonHandler
    {
    public:
        CConfirmBackMsgBoxButtonHandler(CMenuCustomLobby* menu);

    protected:
        static void __fastcall destructor(CConfirmBackMsgBoxButtonHandler* thisptr,
                                          int /*%edx*/,
                                          char flags);
        static void __fastcall handler(CConfirmBackMsgBoxButtonHandler* thisptr,
                                       int /*%edx*/,
                                       game::CMidgardMsgBox* msgBox,
                                       bool okPressed);

    private:
        CMenuCustomLobby* m_menu;
    };
    assert_offset(CConfirmBackMsgBoxButtonHandler, vftable, 0);

    struct CRoomPasswordInterf
        : public game::CPopupDialogInterf
        , public CPopupDialogCustomBase
    {
        CRoomPasswordInterf(CMenuCustomLobby* menu);

    protected:
        static void __fastcall okBtnHandler(CRoomPasswordInterf* thisptr, int /*%edx*/);
        static void __fastcall cancelBtnHandler(CRoomPasswordInterf* thisptr, int /*%edx*/);

    private:
        CMenuCustomLobby* m_menu;
    };
    assert_offset(CRoomPasswordInterf, vftable, 0);

private:
    PeerCallback m_peerCallback;
    RoomsCallback m_roomsCallback;
    game::UiEvent m_roomsUpdateEvent;
    std::vector<RoomInfo> m_rooms;
    game::UiEvent m_usersUpdateEvent;
    std::vector<CNetCustomService::UserInfo> m_users;
    game::Vector<game::SmartPtr<game::IMqImage2>> m_userIcons;
    bool m_userIconsAreLeftOriented;
    game::NetMsgEntryData** m_netMsgEntryData;
    CRoomPasswordInterf* m_roomPasswordDialog;
    SLNet::RoomID m_joiningRoomId;
    std::string m_joiningRoomPassword;
    std::deque<ChatMessage> m_chatMessages;
    std::uint32_t m_chatMessageStock;
    game::UiEvent m_chatMessageRegenEvent;
};

assert_offset(CMenuCustomLobby, vftable, 0);

} // namespace hooks

#endif // MENUCUSTOMLOBBY_H
