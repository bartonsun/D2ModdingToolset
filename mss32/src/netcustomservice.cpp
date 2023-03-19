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

#include "netcustomservice.h"
#include "log.h"
#include "mempool.h"
#include "midgard.h"
#include "mqnetservice.h"
#include "netcustomsession.h"
#include "settings.h"
#include "utils.h"
#include <MessageIdentifiers.h>
#include <fmt/format.h>
#include <mutex>

namespace hooks {

// Hardcoded in client and server
#if 0
static const char titleName[]{"Test Title Name"};
static const char titleSecretKey[]{"Test secret key"};
#else
static const char titleName[]{"Disciples2 Motlin"};
static const char titleSecretKey[]{"Disciples2 Key"};
#endif

const char* serverGuidColumnName{"ServerGuid"};
const char* passwordColumnName{"Password"};

game::IMqNetServiceVftable CNetCustomService::m_vftable = {
    (game::IMqNetServiceVftable::Destructor)destructor,
    (game::IMqNetServiceVftable::HasSessions)hasSessions,
    (game::IMqNetServiceVftable::GetSessions)getSessions,
    (game::IMqNetServiceVftable::CreateSession)createSession,
    (game::IMqNetServiceVftable::JoinSession)joinSession,
};

CNetCustomService* CNetCustomService::create()
{
    using namespace game;

    logDebug("lobby.log", "Get peer instance");
    auto lobbyPeer = NetworkPeer::PeerPtr(SLNet::RakPeerInterface::GetInstance());

    const auto& lobbySettings = userSettings().lobby;
    const auto& clientPort = lobbySettings.client.port;
    SLNet::SocketDescriptor socket{clientPort, nullptr};

    logDebug("lobby.log", fmt::format("Start lobby peer on port {:d}", clientPort));

    if (lobbyPeer->Startup(1, &socket, 1) != SLNet::RAKNET_STARTED) {
        logError("lobby.log", "Failed to start lobby client");
        return nullptr;
    }

    const auto& serverIp = lobbySettings.server.ip;
    const auto& serverPort = lobbySettings.server.port;

    logDebug("lobby.log", fmt::format("Connecting to lobby server with ip '{:s}', port {:d}",
                                      serverIp, serverPort));

    if (lobbyPeer->Connect(serverIp.c_str(), serverPort, nullptr, 0)
        != SLNet::CONNECTION_ATTEMPT_STARTED) {
        logError("lobby.log", "Failed to connect to lobby server");
        return nullptr;
    }

    logDebug("lobby.log", "Allocate CNetCustomService");
    auto netService = (CNetCustomService*)game::Memory::get().allocate(sizeof(CNetCustomService));

    logDebug("lobby.log", "Call placement new");
    new (netService) CNetCustomService(std::move(lobbyPeer));

    logDebug("lobby.log", "CNetCustomService created");
    return netService;
}

bool CNetCustomService::isCustom(const game::IMqNetService* service)
{
    return service && service->vftable == &m_vftable;
}

CNetCustomService::CNetCustomService(NetworkPeer::PeerPtr&& peer)
    : m_lobbyPeer(std::move(peer))
    , m_callbacks(this)
    , m_session{nullptr}
{
    vftable = &m_vftable;
    m_lobbyPeer.addCallback(&m_callbacks);

    logDebug("lobby.log", "Set msg factory");
    m_lobbyClient.SetMessageFactory(&m_lobbyMsgFactory);

    logDebug("lobby.log", "Create callbacks");
    m_lobbyClient.SetCallbackInterface(&m_loggingCallbacks);

    logDebug("lobby.log", "Attach lobby client as a plugin");
    m_lobbyPeer.peer->AttachPlugin(&m_lobbyClient);

    m_lobbyPeer.peer->AttachPlugin(&m_roomsClient);
    m_roomsClient.SetRoomsCallback(&m_roomsLogCallback);
}

CNetCustomSession* CNetCustomService::getSession() const
{
    return m_session;
}

void CNetCustomService::setSession(CNetCustomSession* value)
{
    m_session = value;
}

NetworkPeer& CNetCustomService::getPeer()
{
    return m_lobbyPeer;
}

const std::string& CNetCustomService::getAccountName() const
{
    return m_accountName;
}

const SLNet::SystemAddress& CNetCustomService::getRoomOwnerAddress() const
{
    return m_roomOwnerAddress;
}

void CNetCustomService::setRoomOwnerAddress(const SLNet::SystemAddress& value)
{
    m_roomOwnerAddress = value;
}

bool CNetCustomService::createAccount(const char* accountName,
                                      const char* nickname,
                                      const char* password,
                                      const char* pwdRecoveryQuestion,
                                      const char* pwdRecoveryAnswer)
{
    if (!accountName) {
        logDebug("lobby.log", "Empty account name");
        return false;
    }

    if (!nickname) {
        logDebug("lobby.log", "Empty nick name");
        return false;
    }

    if (!password) {
        logDebug("lobby.log", "Empty password");
        return false;
    }

    if (!pwdRecoveryQuestion) {
        logDebug("lobby.log", "Empty pwd recv question");
        return false;
    }

    if (!pwdRecoveryAnswer) {
        logDebug("lobby.log", "Empty pwd recv answer");
        return false;
    }

    auto msg{m_lobbyMsgFactory.Alloc(SLNet::L2MID_Client_RegisterAccount)};
    auto account{static_cast<SLNet::Client_RegisterAccount*>(msg)};
    if (!account) {
        logDebug("lobby.log", "Can not allocate account message");
        return false;
    }

    account->userName = accountName;
    account->titleName = titleName;

    auto& params = account->createAccountParameters;
    params.firstName = nickname;
    params.lastName = params.firstName;
    params.password = password;
    params.passwordRecoveryQuestion = pwdRecoveryQuestion;
    params.passwordRecoveryAnswer = pwdRecoveryAnswer;

    const auto result{account->PrevalidateInput()};
    if (!result) {
        logDebug("lobby.log", "Wrong account registration data");
    } else {
        m_lobbyClient.SendMsg(account);
        logDebug("lobby.log", "Account registration sent");
    }

    m_lobbyMsgFactory.Dealloc(account);
    return result;
}

bool CNetCustomService::loginAccount(const char* accountName, const char* password)
{
    if (!accountName) {
        return false;
    }

    if (!password) {
        return false;
    }

    auto msg{m_lobbyMsgFactory.Alloc(SLNet::L2MID_Client_Login)};
    auto login{static_cast<SLNet::Client_Login*>(msg)};
    if (!login) {
        logDebug("lobby.log", "Can not allocate login message");
        return false;
    }

    login->userName = accountName;
    login->userPassword = password;

    login->titleName = titleName;
    login->titleSecretKey = titleSecretKey;

    const auto result{login->PrevalidateInput()};
    if (!result) {
        logDebug("lobby.log", "Wrong account data");
    } else {
        m_lobbyClient.SendMsg(login);
        logDebug("lobby.log", "Trying to login");
    }

    m_lobbyMsgFactory.Dealloc(login);
    return result;
}

void CNetCustomService::logoutAccount()
{
    auto msg{m_lobbyMsgFactory.Alloc(SLNet::L2MID_Client_Logoff)};
    auto logoff{static_cast<SLNet::Client_Logoff*>(msg)};
    if (!logoff) {
        logDebug("lobby.log", "Can not allocate logoff message");
        return;
    }

    m_lobbyClient.SendMsg(logoff);
    m_lobbyMsgFactory.Dealloc(logoff);
}

void CNetCustomService::setCurrentLobbyPlayer(const char* accountName)
{
    if (accountName) {
        logDebug("lobby.log", fmt::format("Set current logged account to {:s}", accountName));
        m_accountName = accountName;
    } else {
        logDebug("lobby.log", "Forget current logged account");
        m_accountName.clear();
    }
}

bool CNetCustomService::createRoom(const char* roomName,
                                   const char* serverGuid,
                                   const char* password)
{
    if (!roomName) {
        logDebug("lobby.log", "Could not create a room: no room name provided");
        return false;
    }

    if (!serverGuid) {
        logDebug("lobby.log", "Could not create a room: empty server guid");
        return false;
    }

    SLNet::CreateRoom_Func room{};
    room.gameIdentifier = titleName;
    room.userName = m_accountName.c_str();
    room.resultCode = SLNet::REC_SUCCESS;

    auto& params = room.networkedRoomCreationParameters;
    params.roomName = roomName;
    params.slots.publicSlots = 1;
    params.slots.reservedSlots = 0;
    params.slots.spectatorSlots = 0;

    logDebug("lobby.log",
             fmt::format("Account {:s} is trying to create and enter a room "
                         "with serverGuid {:s}, password {:s}",
                         room.userName.C_String(), serverGuid, (password ? password : "")));

    auto& properties = room.initialRoomProperties;

    const auto guidColumn{
        properties.AddColumn(serverGuidColumnName, DataStructures::Table::STRING)};

    constexpr auto invalidColumn{std::numeric_limits<unsigned int>::max()};
    if (guidColumn == invalidColumn) {
        logDebug("lobby.log",
                 fmt::format("Could not add server guid column to room properties table"));
        return false;
    }

    unsigned int passwordColumn{invalidColumn};
    if (password) {
        passwordColumn = properties.AddColumn(passwordColumnName, DataStructures::Table::STRING);
        if (passwordColumn == invalidColumn) {
            logDebug("lobby.log",
                     fmt::format("Could not add password column to room properties table"));
            return false;
        }
    }

    auto row = properties.AddRow(0);
    if (!row) {
        logDebug("lobby.log", "Could not add row to room properties table");
        return false;
    }

    row->UpdateCell(guidColumn, serverGuid);
    if (password) {
        row->UpdateCell(passwordColumn, password);
    }

    logDebug("lobby.log", fmt::format("Account {:s} is trying to create and enter a room {:s}",
                                      room.userName.C_String(), params.roomName.C_String()));
    m_roomsClient.ExecuteFunc(&room);
    return true;
}

bool CNetCustomService::searchRooms(const char* accountName)
{
    SLNet::SearchByFilter_Func search;
    search.gameIdentifier = titleName;
    search.userName = accountName ? accountName : m_accountName.c_str();

    logDebug("lobby.log",
             fmt::format("Account {:s} is trying to search rooms", search.userName.C_String()));

    m_roomsClient.ExecuteFunc(&search);
    return true;
}

bool CNetCustomService::joinRoom(const char* roomName)
{
    if (!roomName) {
        logDebug("lobby.log", "Could not join room without a name");
        return false;
    }

    SLNet::JoinByFilter_Func join;
    join.gameIdentifier = titleName;
    join.userName = m_accountName.c_str();
    join.query.AddQuery_STRING(DefaultRoomColumns::GetColumnName(DefaultRoomColumns::TC_ROOM_NAME),
                               roomName);
    join.roomMemberMode = RMM_PUBLIC;

    logDebug("lobby.log", fmt::format("Account {:s} is trying to join room {:s}",
                                      join.userName.C_String(), roomName));

    m_roomsClient.ExecuteFunc(&join);
    return true;
}

bool CNetCustomService::changeRoomPublicSlots(unsigned int publicSlots)
{
    if (publicSlots < 1) {
        logDebug("lobby.log", "Could not set number of room public slots lesser than 1");
        return false;
    }

    SLNet::ChangeSlotCounts_Func slotCounts;
    slotCounts.userName = m_accountName.c_str();
    slotCounts.slots.publicSlots = publicSlots;

    logDebug("lobby.log", fmt::format("Account {:s} is trying to change room public slots to {:d}",
                                      slotCounts.userName.C_String(), publicSlots));

    m_roomsClient.ExecuteFunc(&slotCounts);
    return true;
}

bool CNetCustomService::checkFilesIntegrity(const char* hash)
{
    if (!hash || !std::strlen(hash)) {
        logDebug("lobby.log", "Could not check client integrity with empty hash");
        return false;
    }

    SLNet::BitStream stream;
    stream.Write(static_cast<SLNet::MessageID>(ID_CHECK_FILES_INTEGRITY));
    stream.Write(hash);

    const auto serverAddress{m_lobbyClient.GetServerAddress()};
    const auto result{m_lobbyPeer.peer->Send(&stream, PacketPriority::HIGH_PRIORITY,
                                             PacketReliability::RELIABLE_ORDERED, 0, serverAddress,
                                             false)};

    return result != 0;
}

void CNetCustomService::addPeerCallbacks(NetworkPeerCallbacks* callbacks)
{
    m_lobbyPeer.addCallback(callbacks);
}

void CNetCustomService::removePeerCallbacks(NetworkPeerCallbacks* callbacks)
{
    m_lobbyPeer.removeCallback(callbacks);
}

void CNetCustomService::addLobbyCallbacks(SLNet::Lobby2Callbacks* callbacks)
{
    m_lobbyClient.AddCallbackInterface(callbacks);
}

void CNetCustomService::removeLobbyCallbacks(SLNet::Lobby2Callbacks* callbacks)
{
    m_lobbyClient.RemoveCallbackInterface(callbacks);
}

void CNetCustomService::addRoomsCallback(SLNet::RoomsCallback* callback)
{
    logDebug("lobby.log", fmt::format("Adding room callback {:p}", (void*)callback));
    m_roomsClient.AddRoomsCallback(callback);
}

void CNetCustomService::removeRoomsCallback(SLNet::RoomsCallback* callback)
{
    logDebug("lobby.log", fmt::format("Removing room callback {:p}", (void*)callback));
    m_roomsClient.RemoveRoomsCallback(callback);
}

void __fastcall CNetCustomService::destructor(CNetCustomService* thisptr, int /*%edx*/, char flags)
{
    logDebug("lobby.log", "CNetCustomService d-tor called");

    thisptr->~CNetCustomService();

    if (flags & 1) {
        logDebug("lobby.log", "CNetCustomService d-tor frees memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

bool __fastcall CNetCustomService::hasSessions(CNetCustomService* thisptr, int /*%edx*/)
{
    logDebug("lobby.log", "CNetCustomService hasSessions called");
    return false;
}

void __fastcall CNetCustomService::getSessions(CNetCustomService* thisptr,
                                               int /*%edx*/,
                                               game::List<game::IMqNetSessEnum*>* sessions,
                                               const GUID* appGuid,
                                               const char* ipAddress,
                                               bool allSessions,
                                               bool requirePassword)
{
    // This method used by vanilla interface.
    // Since we have our custom one, we can ignore it and there is no need to implement.
    logDebug("lobby.log", "CNetCustomService getSessions called");
}

void __fastcall CNetCustomService::createSession(CNetCustomService* thisptr,
                                                 int /*%edx*/,
                                                 game::IMqNetSession** netSession,
                                                 const GUID* /* appGuid */,
                                                 const char* sessionName,
                                                 const char* password)
{
    logDebug("lobby.log",
             fmt::format("CNetCustomService createSession called. Name '{:s}'", sessionName));

    // We already created a session, just return it
    *netSession = thisptr->m_session;
}

void __fastcall CNetCustomService::joinSession(CNetCustomService* thisptr,
                                               int /*%edx*/,
                                               game::IMqNetSession** netSession,
                                               game::IMqNetSessEnum* netSessionEnum,
                                               const char* password)
{
    // This method is used by vanilla interface.
    // Since we are using our custom one, we can join session directly and ignore this method.
    logDebug("lobby.log", "CNetCustomService joinSession called");
}

void CNetCustomService::Callbacks::onPacketReceived(DefaultMessageIDTypes type,
                                                    SLNet::RakPeerInterface* peer,
                                                    const SLNet::Packet* packet)
{
    if (!m_service->m_lobbyPeer.peer) {
        return;
    }

    switch (type) {
    case ID_DISCONNECTION_NOTIFICATION:
        logDebug("lobby.log", "Disconnected");
        break;
    case ID_ALREADY_CONNECTED:
        logDebug("lobby.log", "Already connected");
        break;
    case ID_CONNECTION_LOST:
        logDebug("lobby.log", "Connection lost");
        break;
    case ID_CONNECTION_ATTEMPT_FAILED:
        logDebug("lobby.log", "Connection attempt failed");
        break;
    case ID_NO_FREE_INCOMING_CONNECTIONS:
        logDebug("lobby.log", "Server is full");
        break;
    case ID_CONNECTION_REQUEST_ACCEPTED: {
        logDebug("lobby.log", "Connection request accepted, set server address");
        // Make sure plugins know about the server
        m_service->m_lobbyClient.SetServerAddress(packet->systemAddress);
        m_service->m_roomsClient.SetServerAddress(packet->systemAddress);
        break;
    }
    case ID_LOBBY2_SERVER_ERROR:
        logDebug("lobby.log", "Lobby server error");
        break;
    default:
        logDebug("lobby.log", fmt::format("Packet type {:d}", static_cast<int>(type)));
        break;
    }
}

static game::IMqNetService* getNetServiceInterface()
{
    auto midgard = game::CMidgardApi::get().instance();
    return midgard->data->netService;
}

CNetCustomService* getNetService()
{
    auto service = getNetServiceInterface();
    if (!isNetServiceCustom(service)) {
        return nullptr;
    }

    return static_cast<CNetCustomService*>(service);
}

bool isNetServiceCustom()
{
    return isNetServiceCustom(getNetServiceInterface());
}

bool isNetServiceCustom(const game::IMqNetService* service)
{
    return CNetCustomService::isCustom(service);
}

} // namespace hooks
