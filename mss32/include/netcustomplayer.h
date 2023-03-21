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

#ifndef NETCUSTOMPLAYER_H
#define NETCUSTOMPLAYER_H

#include "mqnetplayer.h"
#include <cstdint>
#include <string>

namespace game {
struct IMqNetSystem;
struct IMqNetReception;
} // namespace game

namespace hooks {

class CNetCustomService;
class CNetCustomSession;

class CNetCustomPlayer : public game::IMqNetPlayer
{
public:
    CNetCustomPlayer(CNetCustomSession* session,
                     game::IMqNetSystem* system,
                     game::IMqNetReception* reception,
                     const char* name,
                     std::uint32_t id);
    ~CNetCustomPlayer();

    CNetCustomService* getService() const;
    CNetCustomSession* getSession() const;
    game::IMqNetSystem* getSystem() const;
    void setSystem(game::IMqNetSystem* value);
    game::IMqNetReception* getReception() const;
    void setReception(game::IMqNetReception* value);
    const std::string& getName() const;
    void setName(const char* value);
    std::uint32_t getId() const;
    void setId(std::uint32_t value);

protected:
    // IMqNetPlayer
    using GetName = game::String*(__fastcall*)(CNetCustomPlayer*, int, game::String*);
    using GetSession = game::IMqNetSession*(__fastcall*)(CNetCustomPlayer*, int);
    static void __fastcall destructor(CNetCustomPlayer* thisptr, int /*%edx*/, char flags);
    static game::String* __fastcall getName(CNetCustomPlayer* thisptr,
                                            int /*%edx*/,
                                            game::String* string);
    static int __fastcall getNetId(CNetCustomPlayer* thisptr, int /*%edx*/);
    static game::IMqNetSession* __fastcall getSession(CNetCustomPlayer* thisptr, int /*%edx*/);
    static int __fastcall getMessageCount(CNetCustomPlayer* thisptr, int /*%edx*/);
    static bool __fastcall sendMessage(CNetCustomPlayer* thisptr,
                                       int /*%edx*/,
                                       int idTo,
                                       const game::NetMessageHeader* message);
    static game::ReceiveMessageResult __fastcall receiveMessage(CNetCustomPlayer* thisptr,
                                                                int /*%edx*/,
                                                                int* idFrom,
                                                                game::NetMessageHeader* buffer);
    static void __fastcall setNetSystem(CNetCustomPlayer* thisptr,
                                        int /*%edx*/,
                                        game::IMqNetSystem* netSystem);
    static int __fastcall method8(CNetCustomPlayer* thisptr, int /*%edx*/, int a2);

private:
    std::string m_name;
    CNetCustomSession* m_session;
    game::IMqNetSystem* m_system;
    game::IMqNetReception* m_reception;
    std::uint32_t m_id;
};

assert_offset(CNetCustomPlayer, vftable, 0);

} // namespace hooks

#endif // NETCUSTOMPLAYER_H
