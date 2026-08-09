/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WAITINGMOVEPATH_H
#define WAITINGMOVEPATH_H

#include "d2assert.h"
#include "midgardid.h"
#include "task.h"

namespace game {

struct CPhaseGame;
struct CMqPoint;

struct CPathData
{
    CPhaseGame* phaseGame;
    CMidgardID stackId;
};

assert_size(CPathData, 8);

struct CPath
{
    CPathData* data;
};

assert_size(CPath, 4);

namespace WaitingMovementPathApi {

struct Api
{
    using PathConstructor = void(__thiscall*)(CPath* thisptr,
                                              CPhaseGame* phaseGame,
                                              const CMidgardID* stackId);
    using PathDestructor = void(__thiscall*)(CPath* thisptr);
    using PathUpdate = bool(__thiscall*)(CPath* thisptr,
                                         const CMqPoint* mapPosition,
                                         int a3,
                                         bool a4);

    ITaskVftable* taskSelectUnitVftable;
    ITaskVftable* taskWaitVftable;
    ITaskVftable::Destructor taskWaitDestructor;
    PathConstructor pathConstructor;
    PathDestructor pathDestructor;
    PathUpdate pathUpdate;
};

const Api* get();

}

}

#endif
