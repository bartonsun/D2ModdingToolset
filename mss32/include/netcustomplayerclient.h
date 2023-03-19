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

#ifndef NETCUSTOMPLAYERCLIENT_H
#define NETCUSTOMPLAYERCLIENT_H

#include "mqnetplayerclient.h"
#include "netcustomplayer.h"
#include <NatPunchthroughClient.h>
#include <list>
#include <mutex>
#include <slikenet/types.h>
#include <utility>

namespace hooks {

class CNetCustomPlayerClient : public CNetCustomPlayer
{
public:
    static CNetCustomPlayerClient* create(CNetCustomSession* session);
    CNetCustomPlayerClient(CNetCustomSession* session,
                           game::IMqNetSystem* system,
                           game::IMqNetReception* reception,
                           const char* name,
                           NetworkPeer::PeerPtr&& peer,
                           std::uint32_t id,
                           const SLNet::SystemAddress& serverAddress,
                           std::uint32_t serverId);
    ~CNetCustomPlayerClient() = default;

    const SLNet::SystemAddress& getServerAddress() const;
    void setServerAddress(const SLNet::SystemAddress& value);
    std::uint32_t getServerId() const;
    void setServerId(std::uint32_t value);
    SLNet::NatPunchthroughClient& getNatClient();
    void setupPacketCallbacks();
    using CNetCustomPlayer::setName;

protected:
    // IMqNetPlayerClient
    using SetName = bool(__fastcall*)(CNetCustomPlayerClient* thisptr, int, const char* name);
    static void __fastcall destructor(CNetCustomPlayerClient* thisptr, int /*%edx*/, char flags);
    static int __fastcall getMessageCount(CNetCustomPlayerClient* thisptr, int /*%edx*/);
    static bool __fastcall sendMessage(CNetCustomPlayerClient* thisptr,
                                       int /*%edx*/,
                                       int idTo,
                                       const game::NetMessageHeader* message);
    static int __fastcall receiveMessage(CNetCustomPlayerClient* thisptr,
                                         int /*%edx*/,
                                         int* idFrom,
                                         game::NetMessageHeader* buffer);
    static bool __fastcall setName(CNetCustomPlayerClient* thisptr, int /*%edx*/, const char* name);
    static bool __fastcall isHost(CNetCustomPlayerClient* thisptr, int /*%edx*/);

private:
    class Callbacks : public NetworkPeerCallbacks
    {
    public:
        Callbacks(CNetCustomPlayerClient* player)
            : m_player{player}
        { }
        virtual ~Callbacks() = default;

        void onPacketReceived(DefaultMessageIDTypes type,
                              SLNet::RakPeerInterface* peer,
                              const SLNet::Packet* packet) override;

    private:
        CNetCustomPlayerClient* m_player;
    };

    using NetMessagePtr = std::unique_ptr<unsigned char[]>;
    using IdMessagePair = std::pair<std::uint32_t, NetMessagePtr>;

    std::list<IdMessagePair> m_messages;
    std::mutex m_messagesMutex;
    Callbacks m_callbacks;
    SLNet::NatPunchthroughClient m_natClient;
    SLNet::SystemAddress m_serverAddress;
    std::uint32_t m_serverId;
};

assert_offset(CNetCustomPlayerClient, vftable, 0);

} // namespace hooks

#endif // NETCUSTOMPLAYERCLIENT_H
