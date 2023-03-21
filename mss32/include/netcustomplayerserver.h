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
#include <list>
#include <mutex>
#include <vector>

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
    ~CNetCustomPlayerServer() = default;

    bool notifyHostClientConnected();

protected:
    // IMqNetPlayerClient
    static void __fastcall destructor(CNetCustomPlayerServer* thisptr, int /*%edx*/, char flags);
    static int __fastcall getMessageCount(CNetCustomPlayerServer* thisptr, int /*%edx*/);
    static bool __fastcall sendMessage(CNetCustomPlayerServer* thisptr,
                                       int /*%edx*/,
                                       int idTo,
                                       const game::NetMessageHeader* message);
    static game::ReceiveMessageResult __fastcall receiveMessage(CNetCustomPlayerServer* thisptr,
                                                                int /*%edx*/,
                                                                int* idFrom,
                                                                game::NetMessageHeader* buffer);
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
    class Callbacks : public NetPeerCallbacks
    {
    public:
        Callbacks(CNetCustomPlayerServer* player)
            : m_player{player}
        { }
        virtual ~Callbacks() = default;

        void onPacketReceived(DefaultMessageIDTypes type,
                              SLNet::RakPeerInterface* peer,
                              const SLNet::Packet* packet) override;

    private:
        CNetCustomPlayerServer* m_player;
    };

    using NetMessagePtr = std::unique_ptr<unsigned char[]>;
    using IdMessagePair = std::pair<std::uint32_t, NetMessagePtr>;

    Callbacks m_callbacks;
    std::list<IdMessagePair> m_messages;
    std::mutex m_messagesMutex;
    std::vector<SLNet::RakNetGUID> m_connectedIds;
};

assert_offset(CNetCustomPlayerServer, vftable, 0);

} // namespace hooks

#endif // NETCUSTOMPLAYERSERVER_H
