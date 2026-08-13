/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/bartonsun/D2ModdingToolset)
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

#ifndef LOBBYSAVEEXCHANGE_H
#define LOBBYSAVEEXCHANGE_H

#include "netcustomservice.h"
#include <string>

namespace hooks {

/** Lobby-server driven native host-save exchange.
 * A V2 request carries version, saveId, role, maxBytes and timeoutMs. V3 additionally carries
 * mode and a bounded ASCII save stem. Upload responses remain SaveTransferV2 and start with
 * version, saveId and operation, followed by one of:
 * BEGIN(totalSize), CHUNK(offset, size, bytes), COMMIT, or FAIL(code).
 * All fields are written individually through SLNet::BitStream; this is not a packed ABI. */

/** Handles a validated V2/V3 request on the main/UI thread. Only the native host path is valid;
 * the Joiner wire value remains reserved and is rejected. */
void handleLobbySaveRequest(const LobbyProtocol::SaveRequestV3& request);

/** Sends a V2 FAIL operation when a request can be correlated but cannot be accepted. */
void sendLobbySaveFailure(std::uint64_t saveId, LobbyProtocol::SaveFailureV2 failure);

/** Sends the failure on the response channel selected by the request mode. */
void sendLobbySaveFailure(const LobbyProtocol::SaveRequestV3& request,
                          LobbyProtocol::SaveFailureV2 failure);

/** Deletes the exact lobby-owned local save only after a matching authenticated server ACK. */
void handleLobbySaveStoredAck(std::uint64_t saveId);

/** Handles a copied CommandMsgId::GameSaved result. Only the exact lobby-owned host path can
 * complete the active transfer; unrelated quicksave/autosave results are ignored. */
void handleGameSavedForLobby(bool success, const std::string& savePath);

/** Returns true only while a validated Russobit native host-save request is pending. */
bool hasActiveLobbyHostSaveTransfer();

/** Expires pending transfers from the existing 250-ms main-thread lobby watchdog. */
void expireLobbySaveTransfers();

/** Terminates every pending transfer after the lobby's authoritative match deadline. */
void terminateLobbySaveTransfers();

/** Clears all in-memory transfer state without touching any save files. */
void resetLobbySaveTransferState();

} // namespace hooks

#endif // LOBBYSAVEEXCHANGE_H
