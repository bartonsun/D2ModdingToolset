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
#include "waitingmovepath.h"

#include <cstdint>

#include <spdlog/spdlog.h>

namespace hooks {

namespace {

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

struct WaitingPath
{
    game::CPath* path;
    game::CTaskManager* taskManager;
    game::CPhaseGame* phaseGame;
    game::ITask* task;
    game::CMidgardID stackId;
    game::CMidgardID ownerId;
    game::CMidgardID leaderId;
};

struct PendingWaitingPath
{
    game::CTaskManager* taskManager;
    game::CPhaseGame* phaseGame;
    game::CMidgardID stackId;
    game::CMidgardID ownerId;
    game::CMidgardID leaderId;
};

WaitingPath waitingPath{};
PendingWaitingPath pendingWaitingPath{};

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

void destroyWaitingPath(const game::WaitingMovementPathApi::Api& fn)
{
    game::CPath* path = waitingPath.path;
    waitingPath = {};

    if (path) {
        fn.pathDestructor(path);
        game::Memory::get().freeNonZero(path);

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

const game::CMidStack* getLivePreviewStack(game::CTaskManager* taskManager,
                                           game::ITask* task)
{
    using namespace game;

    const auto* taskData = getTaskWaitData(task);
    if (waitingPath.taskManager != taskManager || waitingPath.task != task || !taskData
        || !taskManager || !taskManager->data || taskManager->data->currentTask != task
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

const game::CMidStack* getSelectedStack(const CTaskSelectUnitData* data,
                                        const game::IMidgardObjectMap* objectMap)
{
    using namespace game;

    if (!data || !objectMap) {
        return nullptr;
    }

    const auto* sourcePath = static_cast<const game::CPath*>(data->path);
    if (sourcePath && sourcePath->data && sourcePath->data->phaseGame == data->phaseGame
        && CMidgardIDApi::get().getType(&sourcePath->data->stackId) == IdType::Stack) {
        if (const auto* stack = getStack(objectMap, &sourcePath->data->stackId)) {
            return stack;
        }
    }

    if (CMidgardIDApi::get().getType(&data->selectedObjectId) != IdType::Stack) {
        return nullptr;
    }

    return getStack(objectMap, &data->selectedObjectId);
}

void savePendingWaitingPath(game::CTaskManager* taskManager, CTaskSelectUnit* task)
{
    using namespace game;

    pendingWaitingPath = {};
    if (!taskManager || !task || !task->data || !task->data->phaseGame) {
        return;
    }

    auto* phaseGame = task->data->phaseGame;
    const auto* objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    const auto* stack = getSelectedStack(task->data, objectMap);
    const auto* localPlayerId = getLocalPlayerId();
    if (!stack || !stack->leaderAlive || !localPlayerId || stack->ownerId != *localPlayerId
        || CMidgardIDApi::get().getType(&stack->id) != IdType::Stack) {
        return;
    }

    if (!objectMap->vftable->findScenarioObjectById(objectMap, &stack->leaderId)) {
        return;
    }

    pendingWaitingPath.taskManager = taskManager;
    pendingWaitingPath.phaseGame = phaseGame;
    pendingWaitingPath.stackId = stack->id;
    pendingWaitingPath.ownerId = stack->ownerId;
    pendingWaitingPath.leaderId = stack->leaderId;
    spdlog::debug("Waiting movement path preview selection captured");
}

void activateWaitingPath(game::CTaskManager* taskManager, game::ITask* waitTask)
{
    using namespace game;

    const PendingWaitingPath pending{pendingWaitingPath};
    pendingWaitingPath = {};

    const auto* fn = WaitingMovementPathApi::get();
    const auto* waitTaskData = getTaskWaitData(waitTask);
    if (!fn || pending.taskManager != taskManager || !waitTaskData
        || waitTaskData->phaseGame != pending.phaseGame || !pending.phaseGame) {
        return;
    }

    const auto* objectMap = CPhaseApi::get().getDataCache(&pending.phaseGame->phase);
    const auto* stack = objectMap ? getStack(objectMap, &pending.stackId) : nullptr;
    const auto* localPlayerId = getLocalPlayerId();
    if (!stack || !stack->leaderAlive || !localPlayerId || stack->ownerId != *localPlayerId
        || stack->ownerId != pending.ownerId || stack->leaderId != pending.leaderId
        || !objectMap->vftable->findScenarioObjectById(objectMap, &stack->leaderId)) {
        return;
    }

    destroyWaitingPath(*fn);
    waitingPath.path = nullptr;
    waitingPath.taskManager = taskManager;
    waitingPath.phaseGame = pending.phaseGame;
    waitingPath.task = waitTask;
    waitingPath.stackId = pending.stackId;
    waitingPath.ownerId = pending.ownerId;
    waitingPath.leaderId = pending.leaderId;
    spdlog::debug("Waiting movement path preview attached to wait task");
}

bool ensureWaitingPathCreated()
{
    using namespace game;

    if (waitingPath.path) {
        return true;
    }

    const auto* fn = WaitingMovementPathApi::get();
    if (!fn || !waitingPath.phaseGame) {
        return false;
    }

    auto* path = static_cast<game::CPath*>(Memory::get().allocate(sizeof(game::CPath)));
    if (!path) {
        spdlog::warn("Could not allocate a movement path preview while waiting");
        return false;
    }

    ScopedWaitingMovementPathPreview preview{&waitingPath.ownerId};
    fn->pathConstructor(path, waitingPath.phaseGame, &waitingPath.stackId);

    waitingPath.path = path;
    return true;
}

void __fastcall taskManagerSetCurrentTaskHooked(game::CTaskManager* thisptr,
                                                 int,
                                                 game::ITask* nextTask)
{
    const auto* fn = game::WaitingMovementPathApi::get();
    auto* currentTask = thisptr && thisptr->data ? thisptr->data->currentTask : nullptr;

    if (fn && currentTask == waitingPath.task && currentTask != nextTask) {
        destroyWaitingPath(*fn);
    }

    if (fn && currentTask && currentTask != nextTask
        && currentTask->vftable == fn->taskSelectUnitVftable
        && (!nextTask || nextTask->vftable == fn->taskMsgBoxVftable
            || nextTask->vftable == fn->taskWaitVftable)) {
        savePendingWaitingPath(thisptr, static_cast<CTaskSelectUnit*>(currentTask));
    }

    taskManagerSetCurrentTask(thisptr, nextTask);

    if (!fn || !thisptr || !thisptr->data) {
        return;
    }

    auto* actualTask = thisptr->data->currentTask;
    if (actualTask && actualTask->vftable == fn->taskWaitVftable) {
        if (pendingWaitingPath.taskManager == thisptr) {
            activateWaitingPath(thisptr, actualTask);
        }
        return;
    }

    if (pendingWaitingPath.taskManager == thisptr && actualTask
        && actualTask->vftable != fn->taskSelectUnitVftable
        && actualTask->vftable != fn->taskMsgBoxVftable
        && actualTask->vftable != fn->midNullTaskVftable) {
        pendingWaitingPath = {};
    }
}

void __fastcall taskWaitDestructorHooked(game::ITask* thisptr, int, char flags)
{
    if (waitingPath.task == thisptr) {
        const auto* fn = game::WaitingMovementPathApi::get();
        if (fn) {
            destroyWaitingPath(*fn);
        } else {
            waitingPath = {};
        }
    }

    taskWaitDestructor(thisptr, flags);
}

bool __fastcall taskWaitHandleMouseHooked(game::ITask* thisptr,
                                           int,
                                           std::uint32_t mouseButton,
                                           const game::CMqPoint* mousePosition)
{
    const auto* fn = game::WaitingMovementPathApi::get();
    auto* taskManager = thisptr && thisptr->vftable && thisptr->vftable->getTaskManager
                            ? thisptr->vftable->getTaskManager(thisptr)
                            : nullptr;

    if (fn && mouseButton == leftMouseButtonDown && mousePosition) {
        if (!getLivePreviewStack(taskManager, thisptr)) {
            if (waitingPath.path && waitingPath.task == thisptr) {
                clearMovementPathImages();
            }
            return taskWaitHandleMouse(thisptr, mouseButton, mousePosition);
        }

        if (!ensureWaitingPathCreated()) {
            return taskWaitHandleMouse(thisptr, mouseButton, mousePosition);
        }

        game::CMqPoint mapPosition{};
        if (!convertScreenPositionToMap(mousePosition, &mapPosition)) {
            clearMovementPathImages();
            return taskWaitHandleMouse(thisptr, mouseButton, mousePosition);
        }

        ScopedWaitingMovementPathPreview preview{&waitingPath.ownerId};
        if (fn->pathUpdate(waitingPath.path, &mapPosition, 1, false)) {
            return true;
        }

        clearMovementPathImages();
    }

    return taskWaitHandleMouse(thisptr, mouseButton, mousePosition);
}

}

void addWaitingMovementPathHooks(Hooks& hooks)
{
    const auto* fn = game::WaitingMovementPathApi::get();
    if (!fn) {
        return;
    }

    hooks.emplace_back(HookInfo{game::CTaskManagerApi::get().setCurrentTask,
                                taskManagerSetCurrentTaskHooked,
                                (void**)&taskManagerSetCurrentTask});
    hooks.emplace_back(
        HookInfo{fn->taskWaitDestructor, taskWaitDestructorHooked, (void**)&taskWaitDestructor});
    spdlog::info("Waiting movement path preview hooks registered");
}

void addWaitingMovementPathVftableHooks(Hooks& hooks)
{
    const auto* fn = game::WaitingMovementPathApi::get();
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

}
