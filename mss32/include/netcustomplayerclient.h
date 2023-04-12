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
#include "netcustomservice.h"

namespace hooks {

class CNetCustomPlayerClient : public CNetCustomPlayer
{
public:
    static CNetCustomPlayerClient* create(CNetCustomSession* session,
                                          game::IMqNetSystem* system,
                                          game::IMqNetReception* reception,
                                          const char* name,
                                          const SLNet::RakNetGUID& serverGuid);
    CNetCustomPlayerClient(CNetCustomSession* session,
                           game::IMqNetSystem* system,
                           game::IMqNetReception* reception,
                           const char* name,
                           const SLNet::RakNetGUID& serverGuid);
    ~CNetCustomPlayerClient();

protected:
    using CNetCustomPlayer::sendMessage;
    using CNetCustomPlayer::setName;

    // IMqNetPlayerClient
    using SendNetMessage = bool(__fastcall*)(CNetCustomPlayerClient* thisptr,
                                             int /*%edx*/,
                                             int idTo,
                                             const game::NetMessageHeader* message);
    using SetName = bool(__fastcall*)(CNetCustomPlayerClient* thisptr,
                                      int /*%edx*/,
                                      const char* name);
    static void __fastcall destructor(CNetCustomPlayerClient* thisptr, int /*%edx*/, char flags);
    static bool __fastcall sendMessage(CNetCustomPlayerClient* thisptr,
                                       int /*%edx*/,
                                       int idTo,
                                       const game::NetMessageHeader* message);
    static bool __fastcall setName(CNetCustomPlayerClient* thisptr, int /*%edx*/, const char* name);
    static bool __fastcall isHost(CNetCustomPlayerClient* thisptr, int /*%edx*/);

private:
    class Callbacks : public NetPeerCallbacks
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

    Callbacks m_callbacks;
    SLNet::RakNetGUID m_serverGuid;
};

assert_offset(CNetCustomPlayerClient, vftable, 0);

} // namespace hooks

#endif // NETCUSTOMPLAYERCLIENT_H
