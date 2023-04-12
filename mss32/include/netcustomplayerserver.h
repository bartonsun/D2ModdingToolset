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

#ifndef NETCUSTOMPLAYERSERVER_H
#define NETCUSTOMPLAYERSERVER_H

#include "mqnetplayerserver.h"
#include "netcustomplayer.h"
#include "netcustomservice.h"
#include <set>

namespace hooks {

class CNetCustomPlayerServer : public CNetCustomPlayer
{
public:
    static CNetCustomPlayerServer* create(CNetCustomSession* session,
                                          game::IMqNetSystem* system,
                                          game::IMqNetReception* reception);
    CNetCustomPlayerServer(CNetCustomSession* session,
                           game::IMqNetSystem* system,
                           game::IMqNetReception* reception);
    ~CNetCustomPlayerServer();

    std::set<SLNet::RakNetGUID> getClients() const;
    void addClient(const SLNet::RakNetGUID& guid);
    void removeClient(const SLNet::RakNetGUID& guid);

protected:
    using CNetCustomPlayer::sendMessage;

    // IMqNetPlayerClient
    using SendNetMessage = bool(__fastcall*)(CNetCustomPlayerServer* thisptr,
                                             int /*%edx*/,
                                             int idTo,
                                             const game::NetMessageHeader* message);
    static void __fastcall destructor(CNetCustomPlayerServer* thisptr, int /*%edx*/, char flags);
    static bool __fastcall sendMessage(CNetCustomPlayerServer* thisptr,
                                       int /*%edx*/,
                                       int idTo,
                                       const game::NetMessageHeader* message);
    static bool __fastcall destroyPlayer(CNetCustomPlayerServer* thisptr,
                                         int /*%edx*/,
                                         int playerId);
    static bool __fastcall setMaxPlayers(CNetCustomPlayerServer* thisptr,
                                         int /*%edx*/,
                                         int maxPlayers);
    static bool __fastcall setAllowJoin(CNetCustomPlayerServer* thisptr,
                                        int /*%edx*/,
                                        bool allowJoin);

private:
    class PeerCallbacks : public NetPeerCallbacks
    {
    public:
        PeerCallbacks(CNetCustomPlayerServer* player)
            : m_player{player}
        { }

        void onPacketReceived(DefaultMessageIDTypes type,
                              SLNet::RakPeerInterface* peer,
                              const SLNet::Packet* packet) override;

    private:
        CNetCustomPlayerServer* m_player;
    };

    class RoomsCallback : public SLNet::RoomsCallback
    {
    public:
        RoomsCallback(CNetCustomPlayerServer* player)
            : m_player{player}
        { }

        void CreateRoom_Callback(const SLNet::SystemAddress& senderAddress,
                                 SLNet::CreateRoom_Func* callResult) override;

        void LeaveRoom_Callback(const SLNet::SystemAddress& senderAddress,
                                SLNet::LeaveRoom_Func* callResult) override;

        void RoomMemberLeftRoom_Callback(
            const SLNet::SystemAddress& senderAddress,
            SLNet::RoomMemberLeftRoom_Notification* notification) override;

        void RoomMemberJoinedRoom_Callback(
            const SLNet::SystemAddress& senderAddress,
            SLNet::RoomMemberJoinedRoom_Notification* notification) override;

    private:
        CNetCustomPlayerServer* m_player;
    };

    PeerCallbacks m_peerCallbacks;
    RoomsCallback m_roomsCallback;
    std::set<SLNet::RakNetGUID> m_clients;
    mutable std::mutex m_clientsMutex;
};

assert_offset(CNetCustomPlayerServer, vftable, 0);

} // namespace hooks

#endif // NETCUSTOMPLAYERSERVER_H
