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

#ifndef NETCUSTOMSERVICE_H
#define NETCUSTOMSERVICE_H

#include "lobbycallbacks.h"
#include "mqnetservice.h"
#include "netmsg.h"
#include "roomscallback.h"
#include "uievent.h"
#include <Lobby2Client.h>
#include <Lobby2Message.h>
#include <MessageIdentifiers.h>
#include <RakPeerInterface.h>
#include <RoomsPlugin.h>
#include <mutex>
#include <string>
#include <vector>

namespace game {
struct NetMessageHeader;
} // namespace game

namespace hooks {

extern const char* serverGuidColumnName;
extern const char* passwordColumnName;

// Should not exceed the size of SLNet::MessageID
enum ClientMessages
{
    ID_CHECK_FILES_INTEGRITY = ID_USER_PACKET_ENUM + 1,
    ID_FILES_INTEGRITY_RESULT,
    ID_GAME_MESSAGE_TO_HOST_SERVER,
    ID_GAME_MESSAGE_TO_HOST_CLIENT,
    ID_GAME_MESSAGE = game::netMessageNormalType & 0xff,
};

class CNetCustomSession;

class NetPeerCallbacks
{
public:
    virtual ~NetPeerCallbacks() = default;
    virtual void onPacketReceived(DefaultMessageIDTypes type,
                                  SLNet::RakPeerInterface* peer,
                                  const SLNet::Packet* packet) = 0;
};

class CNetCustomService : public game::IMqNetService
{
public:
    static constexpr std::uint32_t peerShutdownTimeout{100};
    static constexpr std::uint32_t peerProcessInterval{100};
    static constexpr std::uint32_t connectionWaitTimeout{10000};

    static CNetCustomService* create();
    static bool isCustom(const game::IMqNetService* service);

    CNetCustomService(SLNet::RakPeerInterface* peer);
    ~CNetCustomService();

    CNetCustomSession* getSession() const;
    void setSession(CNetCustomSession* value);
    const std::string& getAccountName() const;
    const SLNet::RakNetGUID getPeerGuid() const;
    const SLNet::RakNetGUID getLobbyGuid() const;
    bool send(const SLNet::BitStream& stream, const SLNet::RakNetGUID& to) const;

    /**
     * Tries to register new account using credentials provided.
     * @returns true if register request message was successfully send to the server.
     */
    bool createAccount(const char* accountName,
                       const char* nickname,
                       const char* password,
                       const char* pwdRecoveryQuestion,
                       const char* pwdRecoveryAnswer);

    /**
     * Tries to login user with provided credentials.
     * @returns true if login request message was successfully send to the server.
     */
    bool loginAccount(const char* accountName, const char* password);

    /** Logouts currently logged user. */
    void logoutAccount();

    void setCurrentLobbyPlayer(const char* accountName);

    /** Tries to create and enter a new room. */
    bool createRoom(const char* password = nullptr);

    /** Requests a list of rooms for specified account. */
    bool searchRooms(const char* accountName = nullptr);

    /** Tries to join existing room by its name. */
    bool joinRoom(const char* roomName);

    /** Tries to change number of public slots in current room. */
    bool changeRoomPublicSlots(unsigned int publicSlots);

    /** Tries to request files integrity check from the server. */
    bool checkFilesIntegrity(const char* hash);

    void addPeerCallbacks(NetPeerCallbacks* callbacks);
    void removePeerCallbacks(NetPeerCallbacks* callbacks);

    void addLobbyCallbacks(SLNet::Lobby2Callbacks* callbacks);
    void removeLobbyCallbacks(SLNet::Lobby2Callbacks* callbacks);

    void addRoomsCallback(SLNet::RoomsCallback* callback);
    void removeRoomsCallback(SLNet::RoomsCallback* callback);

protected:
    // IMqNetService
    static game::IMqNetServiceVftable m_vftable;
    static void __fastcall destructor(CNetCustomService* thisptr, int /*%edx*/, char flags);
    static bool __fastcall hasSessions(CNetCustomService* thisptr, int /*%edx*/);
    static void __fastcall getSessions(CNetCustomService* thisptr,
                                       int /*%edx*/,
                                       game::List<game::IMqNetSessEnum*>* sessions,
                                       const GUID* appGuid,
                                       const char* ipAddress,
                                       bool allSessions,
                                       bool requirePassword);
    static void __fastcall createSession(CNetCustomService* thisptr,
                                         int /*%edx*/,
                                         game::IMqNetSession** netSession,
                                         const GUID* /* appGuid */,
                                         const char* sessionName,
                                         const char* password);
    static void __fastcall joinSession(CNetCustomService* thisptr,
                                       int /*%edx*/,
                                       game::IMqNetSession** netSession,
                                       game::IMqNetSessEnum* netSessionEnum,
                                       const char* password);

private:
    class Callbacks : public NetPeerCallbacks
    {
    public:
        Callbacks(CNetCustomService* service)
            : m_service(service)
        { }

        void onPacketReceived(DefaultMessageIDTypes type,
                              SLNet::RakPeerInterface* peer,
                              const SLNet::Packet* packet);

    private:
        CNetCustomService* m_service;
    };

    static void __fastcall peerProcessEventCallback(CNetCustomService* thisptr, int /*%edx*/);
    std::vector<NetPeerCallbacks*> getPeerCallbacks() const;

    Callbacks m_callbacks;
    CNetCustomSession* m_session;
    std::string m_accountName;
    SLNet::SystemAddress m_roomOwnerAddress;
    /** Interacts with lobby server. */
    SLNet::Lobby2Client m_lobbyClient;
    /** Creates network messages. */
    SLNet::Lobby2MessageFactory m_lobbyMsgFactory;
    /** Callbacks only for debug logging. */
    LoggingCallbacks m_loggingCallbacks;
    /** Interacts with lobby server rooms. */
    SLNet::RoomsPlugin m_roomsClient;
    RoomsLoggingCallback m_roomsLogCallback;
    /** Connection with lobby server. */
    SLNet::RakPeerInterface* m_peer;
    game::UiEvent m_peerProcessEvent;
    std::vector<NetPeerCallbacks*> m_peerCallbacks;
    mutable std::mutex m_peerCallbacksMutex;
};

assert_offset(CNetCustomService, vftable, 0);

/** Returns net service if it is present and has type CNetCustomService. */
CNetCustomService* getNetService();

} // namespace hooks

#endif // NETCUSTOMSERVICE_H
