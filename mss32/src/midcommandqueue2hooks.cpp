/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Stanislav Egorov.
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

#include "midcommandqueue2hooks.h"
#include "mempool.h"
#include "netmsgcallbacks.h"
#include "netmsgmapentrycmdmovestackendmsg.h"
#include "originalfunctions.h"
#include <new>

namespace hooks {

game::CMidCommandQueue2::CNMMap* __fastcall netMsgMapConstructorHooked(
    game::CMidCommandQueue2::CNMMap* thisptr,
    int /*%edx*/,
    game::NetMsgCallbacks** netCallbacks,
    game::CMidCommandQueue2* commandQueue)
{
    using namespace game;

    const auto& commandQueueApi = CMidCommandQueue2Api::get();

    auto result = getOriginalFunctions().netMsgMapConstructor(thisptr, netCallbacks, commandQueue);

    auto entry = (CNetMsgMapEntryCmdMoveStackEndMsg*)Memory::get().allocate(
        sizeof(CNetMsgMapEntryCmdMoveStackEndMsg));
    new (entry)
        CNetMsgMapEntryCmdMoveStackEndMsg(thisptr, commandQueueApi.netMsgMapQueueMessageCallback);

    NetMsgApi::get().addEntry(thisptr->netMsgEntryData, (CNetMsgMapEntry*)entry);

    return result;
}

} // namespace hooks
