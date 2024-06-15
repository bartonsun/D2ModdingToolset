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

#include "menucustombase.h"
#include "menuprotocol.h"
#include "netcustomservice.h"
#include "popupdialoginterf.h"

namespace hooks {

class CMenuCustomProtocol
    : public game::CMenuProtocol
    , public CMenuCustomBase
{
public:
    CMenuCustomProtocol(game::CMenuPhase* menuPhase);
    ~CMenuCustomProtocol();

    void createNetCustomServiceStartWaitingConnection();

protected:
    // CInterface
    static void __fastcall destructor(CMenuCustomProtocol* thisptr, int /*%edx*/, char flags);

    void showLoginDialog();
    void hideLoginDialog();
    void showRegisterDialog();
    void hideRegisterDialog();

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
        void MessageResult(SLNet::Client_RegisterAccount* message) override;

    private:
        CMenuCustomProtocol* m_menu;
    };

    struct CLoginAccountInterf
        : public game::CPopupDialogInterf
        , public CPopupDialogCustomBase
    {
        CLoginAccountInterf(CMenuCustomProtocol* menu,
                            const char* dialogName = "DLG_LOGIN_ACCOUNT");

    protected:
        static void __fastcall okBtnHandler(CLoginAccountInterf* thisptr, int /*%edx*/);
        static void __fastcall cancelBtnHandler(CLoginAccountInterf* thisptr, int /*%edx*/);
        static void __fastcall registerBtnHandler(CLoginAccountInterf* thisptr, int /*%edx*/);

    private:
        CMenuCustomProtocol* m_menu;
    };

    struct CRegisterAccountInterf
        : public game::CPopupDialogInterf
        , public CPopupDialogCustomBase
    {
        CRegisterAccountInterf(CMenuCustomProtocol* menu,
                               const char* dialogName = "DLG_REGISTER_ACCOUNT");

    protected:
        static void __fastcall okBtnHandler(CRegisterAccountInterf* thisptr, int /*%edx*/);
        static void __fastcall cancelBtnHandler(CRegisterAccountInterf* thisptr, int /*%edx*/);

    private:
        CMenuCustomProtocol* m_menu;
    };

private:
    PeerCallback m_peerCallback;
    LobbyCallback m_lobbyCallback;
    CLoginAccountInterf* m_loginDialog;
    CRegisterAccountInterf* m_registerDialog;
};

assert_offset(CMenuCustomProtocol, vftable, 0);

} // namespace hooks

#endif // MENUCUSTOMPROTOCOL_H
