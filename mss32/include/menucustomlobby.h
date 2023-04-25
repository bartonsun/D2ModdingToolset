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
#include "uievent.h"
#include <Lobby2Message.h>
#include <RoomsPlugin.h>
#include <string>
#include <vector>

namespace game {
struct NetMsgEntryData;
struct CMenuFlashWait;
struct CMenusAnsInfoMsg;
struct CGameVersionMsg;
} // namespace game

namespace hooks {

class CMenuCustomLobby : public game::CMenuBase
{
public:
    static constexpr char dialogName[] = "DLG_CUSTOM_LOBBY";
    static constexpr std::uint32_t roomListUpdateInterval{5000};

    static CMenuCustomLobby* create(game::CMenuPhase* menuPhase);

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

    static void onRoomPasswordCorrect(CMenuCustomLobby* menu);
    static void onRoomPasswordCancel(CMenuCustomLobby* menu);
    static bool onRoomPasswordEnter(CMenuCustomLobby* menu, const char* password);

private:
    class LobbyCallbacks : public SLNet::Lobby2Callbacks
    {
    public:
        LobbyCallbacks(CMenuCustomLobby* menuLobby)
            : menuLobby{menuLobby}
        { }

        ~LobbyCallbacks() override = default;

        void MessageResult(SLNet::Client_Login* message) override;
        void MessageResult(SLNet::Client_Logoff* message) override;

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
    void deleteWaitMenu();
    void updateAccountText(const char* accountName);
    void updateButtons();
    void initializeButtonsHandlers();
    void showError(const char* message);
    void processLogin(const char* accountName);
    void processLogout();
    void processJoin(const char* roomName, const SLNet::RakNetGUID& serverGuid);
    void setRoomsInfo(std::vector<RoomInfo>&& rooms);
    void processJoinError(const char* message);
    void registerClientPlayerAndJoin();

    game::UiEvent roomsListEvent;
    std::vector<RoomInfo> rooms; // cached data
    LobbyCallbacks uiCallbacks;
    RoomListCallbacks roomsCallbacks;
    game::NetMsgEntryData** netMsgEntryData;
    game::CMenuFlashWait* waitMenu;
    bool loggedIn;
};

assert_offset(CMenuCustomLobby, vftable, 0);

} // namespace hooks

#endif // MENUCUSTOMLOBBY_H
