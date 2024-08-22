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

#ifndef MIDCLIENT_H
#define MIDCLIENT_H

#include "d2list.h"
#include "d2vector.h"
#include "midclientcore.h"
#include "midcommandqueue2.h"
#include "midgardid.h"
#include "playerlistentry.h"
#include "textmessage.h"
#include "uievent.h"

namespace game {

struct CPhase;
struct ILoveChat;
struct INotifyPlayerList;

struct CMidClientData
{
    CPhase* phase;
    bool scenarioStarted;
    char padding[3];
    Vector<TextMessage> textMessages; /**< Chat, player join and disconnect messages. */
    List<ILoveChat*> chatList;        /**< Chat messages subscribers. */
    List<INotifyPlayerList*> notifyPlayerList;
    List<CMidgardID> list3;
    Vector<PlayerListEntry> playerListEntries;
    UiEvent notificationFadeEvent; /**< Restores window notify state. */
    /** Flashes game window each second to notify player, for example when battle starts. */
    UiEvent notificationShowEvent;
};

assert_size(CMidClientData, 136);

struct CMidClient : public CMidCommandQueue2::INotifyCQ
{
    CMidClientCore core;
    CMidClientData* data;
};

assert_size(CMidClient, 16);

} // namespace game

#endif // MIDCLIENT_H
