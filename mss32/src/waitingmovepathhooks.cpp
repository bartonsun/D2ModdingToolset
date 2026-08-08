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

#include "waitingmovepathhooks.h"

#include "d2assert.h"
#include "gameutils.h"
#include "isolayers.h"
#include "mapgraphics.h"
#include "mempool.h"
#include "midgard.h"
#include "midgardid.h"
#include "midgardmapfog.h"
#include "midstack.h"
#include "phasegame.h"
#include "task.h"
#include "taskmanager.h"
#include "version.h"

#include <array>
#include <cstdint>

#include <spdlog/spdlog.h>

namespace hooks {

namespace {

/**
 * Reverse engineered state owned by CTaskSelectUnit. The game destroys it as
 * soon as the current task is replaced with CTaskWait. Keeping the original
 * state alive would also keep an active task alive, so a separate CPath is
 * constructed from the two immutable inputs instead.
 */
struct CTaskSelectUnitData
{
    game::CPhaseGame* phaseGame;
    void* path;
    game::CMidgardID selectedObjectId;
};

assert_size(CTaskSelectUnitData, 12);

struct CTaskSelectUnit : public game::ITask
{
    char unknown[28];
    CTaskSelectUnitData* data;
};

assert_size(CTaskSelectUnit, 36);
assert_offset(CTaskSelectUnit, data, 32);

/** The outer CPath object stores a pointer to its 0x48-byte implementation. */
struct CPathData
{
    game::CPhaseGame* phaseGame;
    game::CMidgardID stackId;
};

assert_size(CPathData, 8);

struct CPath
{
    CPathData* data;
};

assert_size(CPath, 4);

struct CTaskWaitData
{
    char unknown[12];
    game::CPhaseGame* phaseGame;
};

assert_size(CTaskWaitData, 16);
assert_offset(CTaskWaitData, phaseGame, 12);

struct CTaskWait : public game::ITask
{
    char unknown[28];
    CTaskWaitData* data;
};

assert_size(CTaskWait, 36);
assert_offset(CTaskWait, data, 32);

using CPathConstructor = void(__thiscall*)(void* thisptr,
                                           game::CPhaseGame* phaseGame,
                                           const game::CMidgardID* stackId);
using CPathDestructor = void(__thiscall*)(void* thisptr);
using CPathUpdate = bool(__thiscall*)(void* thisptr,
                                      const game::CMqPoint* mapPosition,
                                      int a3,
                                      bool a4);

struct Functions
{
    game::ITaskVftable* taskSelectUnitVftable;
    game::ITaskVftable* taskWaitVftable;
    game::ITaskVftable::Destructor taskWaitDestructor;
    CPathConstructor pathConstructor;
    CPathDestructor pathDestructor;
    CPathUpdate pathUpdate;
};

// clang-format off
const std::array<Functions, 4> functions = {{
    // Akella
    Functions{
        (game::ITaskVftable*)0x6dca1c,
        (game::ITaskVftable*)0x6dcbec,
        (game::ITaskVftable::Destructor)0x4d59dc,
        (CPathConstructor)0x4cccc0,
        (CPathDestructor)0x4cceee,
        (CPathUpdate)0x4cd406,
    },
    // Russobit
    Functions{
        (game::ITaskVftable*)0x6dca1c,
        (game::ITaskVftable*)0x6dcbec,
        (game::ITaskVftable::Destructor)0x4d59dc,
        (CPathConstructor)0x4cccc0,
        (CPathDestructor)0x4cceee,
        (CPathUpdate)0x4cd406,
    },
    // Gog: the required CTaskSelectUnit internals have not been mapped yet.
    Functions{},
    // Scenario editor
    Functions{},
}};
// clang-format on

const Functions* getFunctions()
{
    const auto version = gameVersion();
    if (version == GameVersion::Unknown) {
        return nullptr;
    }

    const auto& value = functions[static_cast<int>(version)];
    return value.taskSelectUnitVftable ? &value : nullptr;
}

struct WaitingPath
{
    CPath* path;
    game::CPhaseGame* phaseGame;
    game::ITask* task;
    game::CMidgardID stackId;
    game::CMidgardID ownerId;
    game::CMidgardID leaderId;
};

WaitingPath waitingPath{};

thread_local bool waitingMovementPathPreview = false;
thread_local const game::CMidgardID* waitingMovementPathPreviewOwnerId = nullptr;

class ScopedWaitingMovementPathPreview
{
public:
    explicit ScopedWaitingMovementPathPreview(const game::CMidgardID* ownerId)
        : previous{waitingMovementPathPreview}
        , previousOwnerId{waitingMovementPathPreviewOwnerId}
    {
        waitingMovementPathPreview = true;
        waitingMovementPathPreviewOwnerId = ownerId;
    }

    ~ScopedWaitingMovementPathPreview()
    {
        waitingMovementPathPreview = previous;
        waitingMovementPathPreviewOwnerId = previousOwnerId;
    }

private:
    bool previous;
    const game::CMidgardID* previousOwnerId;
};

using TaskManagerSetCurrentTask = void(__thiscall*)(game::CTaskManager* taskManager,
                                                    game::ITask* task);
TaskManagerSetCurrentTask taskManagerSetCurrentTask = nullptr;
game::ITaskVftable::Destructor taskWaitDestructor = nullptr;
game::ITaskVftable::HandleMouse taskWaitHandleMouse = nullptr;

constexpr std::uint32_t leftMouseButtonDown = 0x201;

void clearMovementPathImages()
{
    using namespace game;

    CIsoLayer alternateLayer{*isoLayers().symMovePath};
    alternateLayer.value *= 3;

    const auto& mapGraphics = MapGraphicsApi::get();
    mapGraphics.hideLayerImages(isoLayers().symMovePath);
    mapGraphics.hideLayerImages(&alternateLayer);
}

/**
 * CPath::~CPath does not dereference CPhaseGame: it only removes the default
 * movement layer and frees CPath's private storage. Therefore it remains safe
 * to clean the preview at the CTaskWait lifetime boundary.
 */
void destroyWaitingPath(const Functions& fn)
{
    CPath* path = waitingPath.path;
    waitingPath = {};

    if (path) {
        fn.pathDestructor(path);
        game::Memory::get().freeNonZero(path);

        // The movement-display hook also uses an alternate layer when Alt is held,
        // while the original CPath destructor clears only the default one.
        clearMovementPathImages();
    }
}

const CTaskWaitData* getTaskWaitData(const game::ITask* task)
{
    return task ? static_cast<const CTaskWait*>(task)->data : nullptr;
}

const game::CMidgardID* getLocalPlayerId();
bool isStandardSequentialTurnWait(game::CPhaseGame* phaseGame,
                                  const game::CMidgardID* localPlayerId);

/** Returns the saved stack only while it still represents the selected stack. */
const game::CMidStack* getLivePreviewStack(game::ITask* task)
{
    using namespace game;

    const auto* taskData = getTaskWaitData(task);
    if (!waitingPath.path || waitingPath.task != task || !taskData
        || taskData->phaseGame != waitingPath.phaseGame || !waitingPath.phaseGame
        || !waitingPath.phaseGame->data || waitingPath.phaseGame->data->clientTakesTurn) {
        return nullptr;
    }

    const auto* localPlayerId = getLocalPlayerId();
    if (!isStandardSequentialTurnWait(waitingPath.phaseGame, localPlayerId)
        || *localPlayerId != waitingPath.ownerId) {
        return nullptr;
    }

    const auto* objectMap = CPhaseApi::get().getDataCache(&waitingPath.phaseGame->phase);
    if (!objectMap) {
        return nullptr;
    }

    const auto* stack = getStack(objectMap, &waitingPath.stackId);
    if (!stack || stack->id != waitingPath.stackId || stack->ownerId != waitingPath.ownerId
        || !stack->leaderAlive || stack->leaderId != waitingPath.leaderId) {
        return nullptr;
    }

    return objectMap->vftable->findScenarioObjectById(objectMap, &stack->leaderId) ? stack
                                                                                   : nullptr;
}

bool convertScreenPositionToMap(const game::CMqPoint* screenPosition, game::CMqPoint* mapPosition)
{
    using namespace game;

    const auto& mapGraphics = MapGraphicsApi::get();
    MapGraphicsPtr mapGraphicsPtr{};
    mapGraphics.getMapGraphics(&mapGraphicsPtr);

    const bool converted = mapGraphicsPtr.data && mapGraphics.convertMouseToMap
                           && mapGraphics.convertMouseToMap(mapGraphicsPtr.data, screenPosition,
                                                            mapPosition, nullptr);

    mapGraphics.setMapGraphics(&mapGraphicsPtr, nullptr);
    return converted;
}

const game::CMidgardID* getLocalPlayerId()
{
    const auto* midgard = game::CMidgardApi::get().instance();
    if (!midgard || !midgard->data || !midgard->data->netPlayerClientPtr) {
        return nullptr;
    }

    return &midgard->data->netPlayerClientPtr->second;
}

/**
 * The preview belongs only to the classic exclusive-turn flow: the local
 * client has stopped taking its turn and the phase has advanced to another
 * current player. A simultaneous-turn implementation keeps the local player
 * active and therefore must not enter this path.
 */
bool isStandardSequentialTurnWait(game::CPhaseGame* phaseGame,
                                  const game::CMidgardID* localPlayerId)
{
    if (!phaseGame || !phaseGame->data || !localPlayerId
        || phaseGame->data->clientTakesTurn) {
        return false;
    }

    const auto* currentPlayerId = game::CPhaseApi::get().getCurrentPlayerId(&phaseGame->phase);
    return currentPlayerId && *currentPlayerId != *localPlayerId;
}

void savePathForWaitingTask(CTaskSelectUnit* task, game::ITask* waitTask)
{
    using namespace game;

    const auto* fn = getFunctions();
    const auto* waitTaskData = getTaskWaitData(waitTask);
    if (!fn || !task || !task->data || !waitTaskData) {
        return;
    }

    const auto* data = task->data;
    auto* phaseGame = data->phaseGame;

    // CTaskSelectUnit is also discarded when opening ordinary interfaces.
    // Retain a calculator only for the transition to the read-only wait task.
    const auto* sourcePath = static_cast<const CPath*>(data->path);
    if (!phaseGame || !sourcePath || !sourcePath->data
        || sourcePath->data->phaseGame != phaseGame || waitTaskData->phaseGame != phaseGame) {
        return;
    }

    const CMidgardID stackId{sourcePath->data->stackId};
    const auto* objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    const auto* stack = objectMap ? getStack(objectMap, &stackId) : nullptr;
    const auto* localPlayerId = getLocalPlayerId();
    if (!stack || !stack->leaderAlive || !isStandardSequentialTurnWait(phaseGame, localPlayerId)
        || stack->ownerId != *localPlayerId) {
        return;
    }

    const CMidgardID ownerId{stack->ownerId};
    const CMidgardID leaderId{stack->leaderId};
    if (!objectMap->vftable->findScenarioObjectById(objectMap, &leaderId)) {
        return;
    }

    // A captured path is one-shot: an unrelated wait task must never reuse it.
    destroyWaitingPath(*fn);

    auto* path = static_cast<CPath*>(Memory::get().allocate(sizeof(CPath)));
    if (!path) {
        spdlog::warn("Could not allocate a movement path preview while waiting");
        return;
    }

    // CPath copies stackId and retains phaseGame. It does not borrow the
    // destroyed CTaskSelectUnitData object and is used only by the matching
    // CTaskWait lifetime.
    ScopedWaitingMovementPathPreview preview{&ownerId};
    fn->pathConstructor(path, phaseGame, &stackId);

    waitingPath.path = path;
    waitingPath.phaseGame = phaseGame;
    waitingPath.task = waitTask;
    waitingPath.stackId = stackId;
    waitingPath.ownerId = ownerId;
    waitingPath.leaderId = leaderId;
}

void __fastcall taskManagerSetCurrentTaskHooked(game::CTaskManager* thisptr,
                                                int /*%edx*/,
                                                game::ITask* nextTask)
{
    const auto* fn = getFunctions();
    auto* currentTask = thisptr && thisptr->data ? thisptr->data->currentTask : nullptr;

    if (fn && currentTask && nextTask && currentTask->vftable == fn->taskSelectUnitVftable
        && nextTask->vftable == fn->taskWaitVftable) {
        // This exact handoff is the sequential-turn transition. Capturing it
        // here makes the retained path causal: unrelated interface changes
        // cannot later attach it to a CTaskWait instance.
        savePathForWaitingTask(static_cast<CTaskSelectUnit*>(currentTask), nextTask);
    } else if (fn && waitingPath.path && currentTask == waitingPath.task
               && currentTask != nextTask) {
        destroyWaitingPath(*fn);
    }

    taskManagerSetCurrentTask(thisptr, nextTask);
}

void __fastcall taskWaitDestructorHooked(game::ITask* thisptr, int /*%edx*/, char flags)
{
    if (waitingPath.task == thisptr) {
        const auto* fn = getFunctions();
        if (fn) {
            destroyWaitingPath(*fn);
        } else {
            waitingPath = {};
        }
    }

    taskWaitDestructor(thisptr, flags);
}

bool __fastcall taskWaitHandleMouseHooked(game::ITask* thisptr,
                                          int /*%edx*/,
                                          std::uint32_t mouseButton,
                                          const game::CMqPoint* mousePosition)
{
    const auto* fn = getFunctions();

    if (fn && mouseButton == leftMouseButtonDown && mousePosition && getLivePreviewStack(thisptr)) {
        game::CMqPoint mapPosition{};
        if (!convertScreenPositionToMap(mousePosition, &mapPosition)) {
            return taskWaitHandleMouse(thisptr, mouseButton, mousePosition);
        }

        // CPath::update only calculates and draws the route. The normal task's
        // subsequent call to CPhaseGame::sendStackMoveMsg is deliberately not
        // reached from this handler.
        ScopedWaitingMovementPathPreview preview{&waitingPath.ownerId};
        if (fn->pathUpdate(waitingPath.path, &mapPosition, 1, false)) {
            return true;
        }
    }

    return taskWaitHandleMouse(thisptr, mouseButton, mousePosition);
}

} // namespace

void addWaitingMovementPathHooks(Hooks& hooks)
{
    const auto* fn = getFunctions();
    if (!fn) {
        return;
    }

    hooks.emplace_back(HookInfo{game::CTaskManagerApi::get().setCurrentTask,
                                taskManagerSetCurrentTaskHooked,
                                (void**)&taskManagerSetCurrentTask});
    hooks.emplace_back(
        HookInfo{fn->taskWaitDestructor, taskWaitDestructorHooked, (void**)&taskWaitDestructor});
}

void addWaitingMovementPathVftableHooks(Hooks& hooks)
{
    const auto* fn = getFunctions();
    if (!fn) {
        return;
    }

    hooks.emplace_back(HookInfo{&fn->taskWaitVftable->handleMouse, taskWaitHandleMouseHooked,
                                (void**)&taskWaitHandleMouse});
}

bool isWaitingMovementPathPreview()
{
    return waitingMovementPathPreview;
}

bool isWaitingMovementPathPreviewTileVisible(const game::IMidgardObjectMap* objectMap,
                                             const game::CMqPoint* mapPosition)
{
    using namespace game;

    if (!waitingMovementPathPreview) {
        return true;
    }

    if (!objectMap || !mapPosition || !waitingMovementPathPreviewOwnerId) {
        return false;
    }

    const auto* player = getPlayer(objectMap, waitingMovementPathPreviewOwnerId);
    const auto* fog = player ? getFog(objectMap, player) : nullptr;
    if (!fog) {
        return false;
    }

    bool isFogged{true};
    return CMidgardMapFogApi::get().getFog(fog, &isFogged, mapPosition) && !isFogged;
}

bool isWaitingMovementPathPreviewAreaVisible(const game::IMidgardObjectMap* objectMap,
                                             const game::CMqPoint* mapPosition)
{
    if (!waitingMovementPathPreview) {
        return true;
    }

    if (!mapPosition) {
        return false;
    }

    for (int y = mapPosition->y - 1; y <= mapPosition->y + 1; ++y) {
        for (int x = mapPosition->x - 1; x <= mapPosition->x + 1; ++x) {
            const game::CMqPoint position{x, y};
            if (!isWaitingMovementPathPreviewTileVisible(objectMap, &position)) {
                return false;
            }
        }
    }

    return true;
}

} // namespace hooks
