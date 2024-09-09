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

#ifndef PHASEGAME_H
#define PHASEGAME_H

#include "d2list.h"
#include "d2pair.h"
#include "interfmanager.h"
#include "midcommandqueue2.h"
#include "middatacache.h"
#include "phase.h"
#include <cstddef>

namespace game {

struct CMidObjectNotify;
struct CMidObjectLock;
struct CIsoChatDisplay;
struct MapGraphics;
struct IIsoCBScroll;
struct CCityDisplay;
struct CMidAnim2System;
struct CMqPoint;

struct CPhaseGameData
{
    InterfManagerImplPtr interfManager;
    int unknown2;
    CInterface* currentInterface;
    SmartPtr<MapGraphics*> mapGraphics;
    SmartPointer palMapIsoScroller;
    IIsoCBScroll* audioRegionCtrl;
    CMidClient* midClient;
    bool clientTakesTurn;
    char padding[3];
    CMidObjectNotify* midObjectNotify;
    CMidAnim2System* animSystem;
    void* listPtr;
    CCityDisplay* cityDisplay;
    CMidgardID currentPlayerId;
    CMidObjectLock* midObjectLock;
    int unknown13;
    CIsoChatDisplay* isoChatDisplay;
};

assert_size(CPhaseGameData, 76);
assert_offset(CPhaseGameData, midClient, 36);

struct CPhaseGame : public CMidCommandQueue2::INotifyCQ
{
    CMidDataCache2::INotify dataCacheNotify;
    CPhase phase;
    CPhaseGameData* data;
};

assert_size(CPhaseGame, 20);
assert_offset(CPhaseGame, phase, 8);

namespace CPhaseGameApi {

struct Api
{
    /** Returns true if CMidObjectLock is locked, preventing some player actions. */
    using CheckObjectLock = bool(__thiscall*)(CPhaseGame* thisptr);
    CheckObjectLock checkObjectLock;

    using SendStackMoveMsg = void(__thiscall*)(CPhaseGame* thisptr,
                                               const CMidgardID* stackId,
                                               const List<Pair<CMqPoint, int>>* movementPath,
                                               const CMqPoint* startPosition,
                                               const CMqPoint* endPosition);
    SendStackMoveMsg sendStackMoveMsg;
};

Api& get();

} // namespace CPhaseGameApi

} // namespace game

#endif // PHASEGAME_H
