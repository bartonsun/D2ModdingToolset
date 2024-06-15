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

#include "menubase.h"
#include "menucustombase.h"
#include "midmsgboxbuttonhandler.h"
#include "netcustomservice.h"
#include "uievent.h"
#include <Lobby2Message.h>
#include <RoomsPlugin.h>
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
    static constexpr char transitionFromProtoName[] = "TRANS_PROTO2CUSTOMLOBBY";
    static constexpr char transitionFromBlackName[] = "TRANS_BLACK2CUSTOMLOBBY";
    static constexpr std::uint32_t roomListUpdateInterval{5000};

    CMenuCustomLobby(game::CMenuPhase* menuPhase);
    ~CMenuCustomLobby();

protected:
    // CInterface
    static void __fastcall destructor(CMenuCustomLobby* thisptr, int /*%edx*/, char flags);

    static void __fastcall registerAccountBtnHandler(CMenuCustomLobby*, int /*%edx*/);
    static void __fastcall loginAccountBtnHandler(CMenuCustomLobby*, int /*%edx*/);
    static void __fastcall logoutAccountBtnHandler(CMenuCustomLobby*, int /*%edx*/);
    static void __fastcall roomsListSearchHandler(CMenuCustomLobby*, int /*%edx*/);
    static void __fastcall createRoomBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall loadBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall joinRoomBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/);
    static void __fastcall listBoxDisplayHandler(CMenuCustomLobby* thisptr,
                                                 int /*%edx*/,
                                                 game::ImagePointList* contents,
                                                 const game::CMqRect* lineArea,
                                                 int index,
                                                 bool selected);
    static bool __fastcall ansInfoMsgHandler(CMenuCustomLobby* menu,
                                             int /*%edx*/,
                                             const game::CMenusAnsInfoMsg* message,
                                             std::uint32_t idFrom);
    static bool __fastcall gameVersionMsgHandler(CMenuCustomLobby* menu,
                                                 int /*%edx*/,
                                                 const game::CGameVersionMsg* message,
                                                 std::uint32_t idFrom);
    static void __fastcall backBtnHandler(CMenuCustomLobby* thisptr, int /*%edx*/);

    static void onRoomPasswordCorrect(CMenuCustomLobby* menu);
    static void onRoomPasswordCancel(CMenuCustomLobby* menu);
    static bool onRoomPasswordEnter(CMenuCustomLobby* menu, const char* password);

private:
    class PeerCallback : public NetPeerCallback
    {
    public:
        PeerCallback(CMenuCustomLobby* menuLobby)
            : menuLobby{menuLobby}
        { }

        ~PeerCallback() override = default;

        void onPacketReceived(DefaultMessageIDTypes type,
                              SLNet::RakPeerInterface* peer,
                              const SLNet::Packet* packet) override;

    private:
        CMenuCustomLobby* menuLobby;
    };

    class RoomListCallbacks : public SLNet::RoomsCallback
    {
    public:
        RoomListCallbacks(CMenuCustomLobby* menuLobby)
            : menuLobby{menuLobby}
        { }

        ~RoomListCallbacks() override = default;

        void JoinByFilter_Callback(const SLNet::SystemAddress& senderAddress,
                                   SLNet::JoinByFilter_Func* callResult) override;

        void SearchByFilter_Callback(const SLNet::SystemAddress& senderAddress,
                                     SLNet::SearchByFilter_Func* callResult) override;

    private:
        CMenuCustomLobby* menuLobby;
    };

    struct CConfirmBackMsgBoxButtonHandler : public game::CMidMsgBoxButtonHandler
    {
    public:
        CConfirmBackMsgBoxButtonHandler(CMenuCustomLobby* menuLobby);

    protected:
        static void __fastcall destructor(CConfirmBackMsgBoxButtonHandler* thisptr,
                                          int /*%edx*/,
                                          char flags);
        static void __fastcall handler(CConfirmBackMsgBoxButtonHandler* thisptr,
                                       int /*%edx*/,
                                       game::CMidgardMsgBox* msgBox,
                                       bool okPressed);

    private:
        CMenuCustomLobby* menuLobby;
    };
    assert_offset(CConfirmBackMsgBoxButtonHandler, vftable, 0);

    struct RoomInfo
    {
        std::string name;
        std::string hostName;
        std::string password;
        int totalSlots;
        int usedSlots;
    };

    int getCurrentRoomIndex();
    void tryJoinRoom(const char* roomName);
    void updateAccountText(const char* accountName);
    void initializeButtonsHandlers();
    void showError(const char* message);
    void processJoin(const char* roomName, const SLNet::RakNetGUID& serverGuid);
    void setRoomsInfo(std::vector<RoomInfo>&& value);
    void processJoinError(const char* message);
    void registerClientPlayerAndJoin();

    PeerCallback peerCallback;
    game::UiEvent roomsListEvent;
    std::vector<RoomInfo> rooms;
    RoomListCallbacks roomsCallbacks;
    game::NetMsgEntryData** netMsgEntryData;
};

assert_offset(CMenuCustomLobby, vftable, 0);

} // namespace hooks

#endif // MENUCUSTOMLOBBY_H
