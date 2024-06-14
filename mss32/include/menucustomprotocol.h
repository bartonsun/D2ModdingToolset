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

#ifndef MENUCUSTOMPROTOCOL_H
#define MENUCUSTOMPROTOCOL_H

#include "menuprotocol.h"
#include "netcustomservice.h"

namespace game {
struct CMenuFlashWait;
} // namespace game

namespace hooks {

class CMenuCustomProtocol : public game::CMenuProtocol
{
public:
    CMenuCustomProtocol(game::CMenuPhase* menuPhase);
    ~CMenuCustomProtocol();

    void createNetCustomServiceStartWaitingConnection();

protected:
    // CInterface
    static void __fastcall destructor(CMenuCustomProtocol* thisptr, int /*%edx*/, char flags);

private:
    class PeerCallback : public NetPeerCallback
    {
    public:
        PeerCallback(CMenuCustomProtocol* menu)
            : m_menu{menu}
        { }

        ~PeerCallback() override = default;

        void onPacketReceived(DefaultMessageIDTypes type,
                              SLNet::RakPeerInterface* peer,
                              const SLNet::Packet* packet) override;

    private:
        CMenuCustomProtocol* m_menu;
    };

    class LobbyCallback : public SLNet::Lobby2Callbacks
    {
    public:
        LobbyCallback(CMenuCustomProtocol* menu)
            : m_menu{menu}
        { }

        ~LobbyCallback() override = default;

        void MessageResult(SLNet::Client_Login* message) override;

    private:
        CMenuCustomProtocol* m_menu;
    };

    void stopWaitingConnection();
    void stopWaitingConnection(const char* errorMessage);

    game::CMenuFlashWait* m_menuWait;
    PeerCallback m_peerCallback;
    LobbyCallback m_lobbyCallback;
};

assert_offset(CMenuCustomProtocol, vftable, 0);

} // namespace hooks

#endif // MENUCUSTOMPROTOCOL_H
