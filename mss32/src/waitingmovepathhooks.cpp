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
#include "fortification.h"
#include "game.h"
#include "gamesettings.h"
#include "gameutils.h"
#include "isolayers.h"
#include "mapgraphics.h"
#include "mempool.h"
#include "midclient.h"
#include "middiplomacy.h"
#include "midgard.h"
#include "midgardid.h"
#include "midgardmapfog.h"
#include "midgardplan.h"
#include "midplayer.h"
#include "midstack.h"
#include "movepathhooks.h"
#include "mqpoint.h"
#include "mquikernelsimple.h"
#include "phasegame.h"
#include "racetype.h"
#include "smartptr.h"
#include "task.h"
#include "taskmanager.h"
#include "uievent.h"
#include "uimanager.h"
#include "utils.h"
#include "waitingmovepath.h"
#include "waitingmovepathplanner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

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

struct CTaskSelectCityData
{
    game::CPhaseGame* phaseGame;
    game::CMidgardID fortId;
    game::CMqPoint position;
    char internal[8];
};

assert_size(CTaskSelectCityData, 24);

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
    game::CPhaseGame* phaseGame;
    const game::CPhase* phase;
    game::CMidgardID stackId;
    game::CMidgardID ownerId;
    game::CMidgardID leaderId;
    game::CMqPoint lastMapPosition;
    int lastPathMode;
    bool lastMapPositionValid;
    bool lastPathModeValid;
    bool updateObserved;
    bool liveStackValidated;
    bool opponentTurnObserved;
    bool lastUpdateSucceeded;
    bool pathUpdateSucceededLogged;
    bool pathUpdateFailedLogged;
    bool liveStateClearLogged;
    bool leftMouseButtonDown;
    bool rightMouseButtonDown;
};

struct BattlePreviewContext
{
    game::CMqPoint anchor;
    game::CMidgardID targetId;
    game::CMqPoint targetPosition;
    int remainingMovement;
    int maxMovement;
    bool valid;
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
game::UiEvent waitingPathUpdateEvent{};
bool waitingPathUpdateEventActive = false;
BattlePreviewContext battlePreviewContext{};
WaitingMovementPath secondSegmentPath{};

thread_local bool waitingMovementPathPreview = false;
thread_local bool waitingMovementPathSecondSegmentPreview = false;
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

class ScopedWaitingMovementSecondSegment
{
public:
    ScopedWaitingMovementSecondSegment()
        : previous{waitingMovementPathSecondSegmentPreview}
    {
        waitingMovementPathSecondSegmentPreview = true;
    }

    ~ScopedWaitingMovementSecondSegment()
    {
        waitingMovementPathSecondSegmentPreview = previous;
    }

private:
    bool previous;
};

using TaskManagerSetCurrentTask = void(__thiscall*)(game::CTaskManager* taskManager,
                                                    game::ITask* task);
TaskManagerSetCurrentTask taskManagerSetCurrentTask = nullptr;

void stopWaitingPathUpdateEvent();
bool startWaitingPathUpdateEvent();

void clearMovementPathImages()
{
    using namespace game;

    CIsoLayer alternateLayer{*isoLayers().symMovePath};
    alternateLayer.value *= 3;
    CIsoLayer secondSegmentLayer{*isoLayers().symMovePath};
    secondSegmentLayer.value *= 4;

    const auto& mapGraphics = MapGraphicsApi::get();
    mapGraphics.hideLayerImages(isoLayers().symMovePath);
    mapGraphics.hideLayerImages(&alternateLayer);
    mapGraphics.hideLayerImages(&secondSegmentLayer);
}

void clearSecondSegmentImages()
{
    game::CIsoLayer layer{*game::isoLayers().symMovePath};
    layer.value *= 4;
    game::MapGraphicsApi::get().hideLayerImages(&layer);
}

bool clearSecondSegmentPreview()
{
    const bool wasVisible = !secondSegmentPath.empty();
    secondSegmentPath.clear();
    clearSecondSegmentImages();
    return wasVisible;
}

bool resetBattlePreview()
{
    battlePreviewContext = {};
    return clearSecondSegmentPreview();
}

void destroyWaitingPath(const game::WaitingMovementPathApi::Api& fn, const char* reason)
{
    stopWaitingPathUpdateEvent();

    game::CPath* path = waitingPath.path;
    const bool attached = waitingPath.phase != nullptr;
    waitingPath = {};
    battlePreviewContext = {};
    secondSegmentPath.clear();

    if (attached) {
        spdlog::debug("Waiting movement path preview stopped: {}", reason);
    }

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

bool isWaitingPathPhaseCurrent()
{
    const auto* midgard = game::CMidgardApi::get().instance();
    const auto* client = midgard && midgard->data ? midgard->data->client : nullptr;
    return client && client->data && client->data->phase == waitingPath.phase;
}

const game::CMidStack* getLivePreviewStack()
{
    using namespace game;

    if (!isWaitingPathPhaseCurrent() || !waitingPath.phaseGame || !waitingPath.phaseGame->data
        || waitingPath.phaseGame->data->clientTakesTurn) {
        return nullptr;
    }

    const auto* localPlayerId = getLocalPlayerId();
    if (!localPlayerId || *localPlayerId != waitingPath.ownerId) {
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

const game::CMidStack* getFortifiedStack(const game::IMidgardObjectMap* objectMap,
                                         const game::CFortification* fort,
                                         const game::CMidgardID* localPlayerId)
{
    using namespace game;

    if (!objectMap || !fort || !localPlayerId || fort->ownerId != *localPlayerId
        || CMidgardIDApi::get().getType(&fort->stackId) != IdType::Stack) {
        return nullptr;
    }

    const auto* stack = getStack(objectMap, &fort->stackId);
    if (!stack || !stack->leaderAlive || stack->ownerId != *localPlayerId
        || stack->insideId != fort->id
        || !objectMap->vftable->findScenarioObjectById(objectMap, &stack->leaderId)) {
        return nullptr;
    }

    return stack;
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
    const auto* localPlayerId = getLocalPlayerId();
    if (!objectMap || !localPlayerId) {
        return;
    }

    pendingWaitingPath.taskManager = taskManager;
    pendingWaitingPath.phaseGame = phaseGame;
    pendingWaitingPath.stackId = emptyId;
    pendingWaitingPath.ownerId = *localPlayerId;
    pendingWaitingPath.leaderId = emptyId;

    const auto* stack = getSelectedStack(task->data, objectMap);
    if (!stack || !stack->leaderAlive || stack->ownerId != *localPlayerId
        || CMidgardIDApi::get().getType(&stack->id) != IdType::Stack
        || !objectMap->vftable->findScenarioObjectById(objectMap, &stack->leaderId)) {
        return;
    }

    pendingWaitingPath.stackId = stack->id;
    pendingWaitingPath.leaderId = stack->leaderId;
    spdlog::debug("Waiting movement path preview selection captured");
}

void savePendingFortifiedStack(game::CTaskManager* taskManager, game::CPhaseGame* phaseGame)
{
    using namespace game;

    pendingWaitingPath = {};
    if (!taskManager || !phaseGame) {
        return;
    }

    const auto* objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    const auto* localPlayerId = getLocalPlayerId();
    if (!objectMap || !localPlayerId) {
        return;
    }

    pendingWaitingPath.taskManager = taskManager;
    pendingWaitingPath.phaseGame = phaseGame;
    pendingWaitingPath.stackId = emptyId;
    pendingWaitingPath.ownerId = *localPlayerId;
    pendingWaitingPath.leaderId = emptyId;

    CMidgardID playerId{*localPlayerId};
    const auto* capital = gameFunctions().findCapitalByPlayerId(&playerId,
                                                                const_cast<CMidDataCache2*>(
                                                                    objectMap));
    const CMidStack* selectedStack = getFortifiedStack(objectMap, capital, localPlayerId);

    const CMidStack* otherStack{};
    int otherStacks{};
    if (!selectedStack) {
        forEachScenarioObject(objectMap, IdType::Stack, [&](const IMidScenarioObject* object) {
            if (!object) {
                return;
            }

            const auto* stack = getStack(objectMap, &object->id);
            if (!stack || stack->ownerId != *localPlayerId || !stack->leaderAlive
                || CMidgardIDApi::get().getType(&stack->insideId) != IdType::Fortification) {
                return;
            }

            const auto* fort = getFort(objectMap, &stack->insideId);
            if (getFortifiedStack(objectMap, fort, localPlayerId) != stack) {
                return;
            }

            ++otherStacks;
            otherStack = stack;
        });
        selectedStack = otherStacks == 1 ? otherStack : nullptr;
    }

    if (!selectedStack) {
        return;
    }

    pendingWaitingPath.stackId = selectedStack->id;
    pendingWaitingPath.leaderId = selectedStack->leaderId;
    spdlog::debug("Waiting movement path preview fortified stack captured");
}

void savePendingSelectedCity(game::CTaskManager* taskManager, game::ITask* task)
{
    using namespace game;

    pendingWaitingPath = {};
    if (!taskManager || !task) {
        return;
    }

    const auto* data = *reinterpret_cast<CTaskSelectCityData* const*>(
        reinterpret_cast<const char*>(task) + 28);
    if (!data || !data->phaseGame
        || CMidgardIDApi::get().getType(&data->fortId) != IdType::Fortification) {
        return;
    }

    const auto* objectMap = CPhaseApi::get().getDataCache(&data->phaseGame->phase);
    const auto* fort = objectMap ? getFort(objectMap, &data->fortId) : nullptr;
    const auto* localPlayerId = getLocalPlayerId();
    const auto* stack = getFortifiedStack(objectMap, fort, localPlayerId);
    if (!stack) {
        return;
    }

    pendingWaitingPath.taskManager = taskManager;
    pendingWaitingPath.phaseGame = data->phaseGame;
    pendingWaitingPath.stackId = stack->id;
    pendingWaitingPath.ownerId = stack->ownerId;
    pendingWaitingPath.leaderId = stack->leaderId;
    spdlog::debug("Waiting movement path preview city stack captured");
}

void activateWaitingPath(game::CTaskManager* taskManager, game::ITask* waitTask)
{
    using namespace game;

    const PendingWaitingPath pending{pendingWaitingPath};
    pendingWaitingPath = {};

    const auto* fn = WaitingMovementPathApi::get();
    const auto* waitTaskData = getTaskWaitData(waitTask);
    if (!fn || pending.taskManager != taskManager || !waitTaskData
        || waitTaskData->phaseGame != pending.phaseGame || !pending.phaseGame
        || !pending.phaseGame->data) {
        return;
    }

    const auto* midgard = CMidgardApi::get().instance();
    const auto* client = midgard && midgard->data ? midgard->data->client : nullptr;
    const auto* currentPhase = client && client->data ? client->data->phase : nullptr;
    if (!currentPhase || currentPhase != &pending.phaseGame->phase) {
        return;
    }

    const auto* localPlayerId = getLocalPlayerId();
    if (!localPlayerId || *localPlayerId != pending.ownerId) {
        return;
    }

    const auto* objectMap = CPhaseApi::get().getDataCache(&pending.phaseGame->phase);
    const bool stackSelected = CMidgardIDApi::get().getType(&pending.stackId) == IdType::Stack;
    if (stackSelected) {
        const auto* stack = objectMap ? getStack(objectMap, &pending.stackId) : nullptr;
        if (!stack || !stack->leaderAlive || stack->ownerId != *localPlayerId
            || stack->ownerId != pending.ownerId || stack->leaderId != pending.leaderId
            || !objectMap->vftable->findScenarioObjectById(objectMap, &stack->leaderId)) {
            return;
        }
    }

    destroyWaitingPath(*fn, "replaced");
    waitingPath.path = nullptr;
    waitingPath.phaseGame = pending.phaseGame;
    waitingPath.phase = currentPhase;
    waitingPath.stackId = pending.stackId;
    waitingPath.ownerId = pending.ownerId;
    waitingPath.leaderId = pending.leaderId;
    waitingPath.leftMouseButtonDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    waitingPath.rightMouseButtonDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    spdlog::debug("Waiting movement path preview attached to wait task");

    if (!startWaitingPathUpdateEvent()) {
        spdlog::warn("Could not start movement path preview updates while waiting");
        destroyWaitingPath(*fn, "update event failed");
    }
}

bool trySelectWaitingPreviewStack(const game::CMqPoint* screenPosition, bool* visualChanged)
{
    using namespace game;

    if (visualChanged) {
        *visualChanged = false;
    }

    if (!screenPosition || !waitingPath.phase || !waitingPath.phaseGame
        || !waitingPath.phaseGame->data || waitingPath.phaseGame->data->clientTakesTurn
        || !isWaitingPathPhaseCurrent()) {
        return false;
    }

    const auto* localPlayerId = getLocalPlayerId();
    const auto* objectMap = CPhaseApi::get().getDataCache(&waitingPath.phaseGame->phase);
    const auto* plan = objectMap ? gameFunctions().getMidgardPlan(objectMap) : nullptr;
    if (!localPlayerId || !objectMap || !plan || waitingPath.ownerId != *localPlayerId) {
        return false;
    }

    CMqPoint mapPosition{};
    if (!convertScreenPositionToMap(screenPosition, &mapPosition)) {
        return false;
    }

    ScopedWaitingMovementPathPreview preview{localPlayerId};
    if (!isWaitingMovementPathPreviewTileVisible(objectMap, &mapPosition)) {
        return false;
    }

    const IdType stackType = IdType::Stack;
    const auto* stackId = CMidgardPlanApi::get().getObjectId(plan, &mapPosition, &stackType);
    const auto* stack = stackId ? getStack(objectMap, stackId) : nullptr;
    if (!stack || stack->ownerId != *localPlayerId || !stack->leaderAlive
        || CMidgardIDApi::get().getType(&stack->id) != IdType::Stack
        || !objectMap->vftable->findScenarioObjectById(objectMap, &stack->leaderId)) {
        return false;
    }
    if (stack->id == waitingPath.stackId) {
        return true;
    }

    const bool hadVisiblePath = waitingPath.lastUpdateSucceeded || !secondSegmentPath.empty();
    if (waitingPath.path) {
        const auto* fn = WaitingMovementPathApi::get();
        if (!fn) {
            return false;
        }

        fn->pathDestructor(waitingPath.path);
        Memory::get().freeNonZero(waitingPath.path);
        waitingPath.path = nullptr;
    }

    resetBattlePreview();
    clearMovementPathImages();
    waitingPath.stackId = stack->id;
    waitingPath.ownerId = stack->ownerId;
    waitingPath.leaderId = stack->leaderId;
    waitingPath.lastMapPositionValid = false;
    waitingPath.lastPathModeValid = false;
    waitingPath.liveStackValidated = false;
    waitingPath.lastUpdateSucceeded = false;
    waitingPath.pathUpdateSucceededLogged = false;
    waitingPath.pathUpdateFailedLogged = false;
    waitingPath.liveStateClearLogged = false;
    if (visualChanged) {
        *visualChanged = hadVisiblePath;
    }
    return true;
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
    spdlog::debug("Waiting movement path preview path created");
    return true;
}

int getWaitingPathMode()
{
    const auto* midgard = game::CMidgardApi::get().instance();
    const auto* settings = midgard && midgard->data && midgard->data->settings
                               ? *midgard->data->settings
                               : nullptr;
    const bool defaultPathBattle = settings && settings->defaultPathBattle;
    const bool controlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    return controlPressed == defaultPathBattle ? 1 : 0;
}

bool applyWaitingPathUpdate(const game::WaitingMovementPathApi::Api& fn,
                            const game::CMqPoint& mapPosition,
                            int pathMode)
{
    const bool secondSegmentWasVisible = resetBattlePreview();
    if (!ensureWaitingPathCreated()) {
        return secondSegmentWasVisible;
    }

    ScopedWaitingMovementPathPreview preview{&waitingPath.ownerId};
    const bool wasVisible = waitingPath.lastUpdateSucceeded || secondSegmentWasVisible;
    const bool updated = fn.pathUpdate(waitingPath.path, &mapPosition, pathMode, false);

    waitingPath.lastMapPosition = mapPosition;
    waitingPath.lastPathMode = pathMode;
    waitingPath.lastMapPositionValid = true;
    waitingPath.lastPathModeValid = true;
    waitingPath.lastUpdateSucceeded = updated;
    waitingPath.liveStateClearLogged = false;

    if (updated) {
        if (!waitingPath.pathUpdateSucceededLogged) {
            spdlog::debug("Waiting movement path preview path updated");
        }
        waitingPath.pathUpdateSucceededLogged = true;
        return true;
    }

    if (!waitingPath.pathUpdateFailedLogged) {
        spdlog::debug("Waiting movement path preview path update failed");
    }
    waitingPath.pathUpdateFailedLogged = true;
    clearMovementPathImages();
    return wasVisible;
}

bool updateWaitingPathPreview(const game::CMqPoint* screenPosition)
{
    const auto* fn = game::WaitingMovementPathApi::get();
    if (!fn || !screenPosition || !waitingPath.phase) {
        return false;
    }

    if (!waitingPath.updateObserved) {
        waitingPath.updateObserved = true;
        spdlog::debug("Waiting movement path preview update received");
    }

    const auto* stack = getLivePreviewStack();
    if (!stack) {
        const bool visualChanged = waitingPath.lastUpdateSucceeded;
        if (visualChanged) {
            clearMovementPathImages();
            if (!waitingPath.liveStateClearLogged) {
                spdlog::debug("Waiting movement path preview cleared: live state rejected");
                waitingPath.liveStateClearLogged = true;
            }
        }
        waitingPath.lastMapPositionValid = false;
        waitingPath.lastUpdateSucceeded = false;
        return visualChanged;
    }

    if (!waitingPath.liveStackValidated) {
        waitingPath.liveStackValidated = true;
        spdlog::debug("Waiting movement path preview live stack validated");
    }

    game::CMqPoint mapPosition{};
    if (!convertScreenPositionToMap(screenPosition, &mapPosition)) {
        return false;
    }

    const bool samePosition = waitingPath.lastMapPositionValid
                              && waitingPath.lastMapPosition.x == mapPosition.x
                              && waitingPath.lastMapPosition.y == mapPosition.y;
    int pathMode = getWaitingPathMode();
    const auto* objectMap = game::CPhaseApi::get().getDataCache(&waitingPath.phaseGame->phase);
    if (objectMap) {
        ScopedWaitingMovementPathPreview preview{&waitingPath.ownerId};
        const auto* plan = game::gameFunctions().getMidgardPlan(objectMap);
        const game::IdType stackType = game::IdType::Stack;
        const auto* targetId = plan ? game::CMidgardPlanApi::get().getObjectId(plan, &mapPosition,
                                                                               &stackType)
                                    : nullptr;
        const auto* target = targetId ? getStack(objectMap, targetId) : nullptr;
        if (target && !target->invisible && target->ownerId != stack->ownerId
            && isWaitingMovementPathPreviewTileVisible(objectMap, &target->position)) {
            pathMode = 0;
        }

        const game::IdType ruinType = game::IdType::Ruin;
        const auto* ruinId = plan ? game::CMidgardPlanApi::get().getObjectId(
                                       plan, &mapPosition, &ruinType)
                                  : nullptr;
        if (ruinId && getRuin(objectMap, ruinId)
            && isWaitingMovementPathPreviewTileVisible(objectMap, &mapPosition)) {
            pathMode = 0;
        }
    }

    if (samePosition && waitingPath.lastUpdateSucceeded && waitingPath.lastPathModeValid
        && waitingPath.lastPathMode == pathMode) {
        return false;
    }

    return applyWaitingPathUpdate(*fn, mapPosition, pathMode);
}

bool updateSecondSegmentPreview(const game::CMqPoint* screenPosition)
{
    using namespace game;

    if (!screenPosition || !battlePreviewContext.valid) {
        return false;
    }

    const auto* stack = getLivePreviewStack();
    const auto* objectMap = stack && waitingPath.phaseGame
                                ? CPhaseApi::get().getDataCache(&waitingPath.phaseGame->phase)
                                : nullptr;
    if (!stack || !objectMap) {
        return clearSecondSegmentPreview();
    }

    CMqPoint destination{};
    if (!convertScreenPositionToMap(screenPosition, &destination)) {
        return false;
    }

    ScopedWaitingMovementPathPreview preview{&waitingPath.ownerId};
    if (!isWaitingMovementPathPreviewTileVisible(objectMap, &destination)) {
        return false;
    }

    const auto& fn = gameFunctions();
    const auto* plan = fn.getMidgardPlan(objectMap);
    if (!plan) {
        return false;
    }

    const auto targetType = CMidgardIDApi::get().getType(&battlePreviewContext.targetId);
    if (targetType == IdType::Stack) {
        const auto* firstTarget = getStack(objectMap, &battlePreviewContext.targetId);
        if (!firstTarget || firstTarget->invisible
            || !isWaitingMovementPathPreviewTileVisible(objectMap, &firstTarget->position)
            || std::abs(firstTarget->position.x - battlePreviewContext.anchor.x) > 1
            || std::abs(firstTarget->position.y - battlePreviewContext.anchor.y) > 1) {
            battlePreviewContext = {};
            return clearSecondSegmentPreview();
        }
    } else if (targetType == IdType::Ruin) {
        const auto* ruin = getRuin(objectMap, &battlePreviewContext.targetId);
        const IdType ruinType = IdType::Ruin;
        const auto* ruinId = CMidgardPlanApi::get().getObjectId(
            plan, &battlePreviewContext.targetPosition, &ruinType);
        CMqPoint entrance{};
        if (!ruin || !ruinId || *ruinId != battlePreviewContext.targetId
            || !isWaitingMovementPathPreviewTileVisible(objectMap,
                                                        &battlePreviewContext.targetPosition)
            || !fn.getFortOrRuinEntrance(objectMap, plan, stack,
                                          &battlePreviewContext.targetPosition, &entrance)
            || !isWaitingMovementPathPreviewTileVisible(objectMap, &entrance)
            || std::abs(entrance.x - battlePreviewContext.anchor.x) > 1
            || std::abs(entrance.y - battlePreviewContext.anchor.y) > 1) {
            battlePreviewContext = {};
            return clearSecondSegmentPreview();
        }
    } else {
        battlePreviewContext = {};
        return clearSecondSegmentPreview();
    }

    const auto* ignoredStackId = targetType == IdType::Stack
                                     ? &battlePreviewContext.targetId
                                     : nullptr;

    const IdType stackType = IdType::Stack;
    const auto* clickedStackId = CMidgardPlanApi::get().getObjectId(plan, &destination, &stackType);
    const CMidStack* clickedStack = clickedStackId ? getStack(objectMap, clickedStackId) : nullptr;
    const auto* stackPlayer = getPlayer(objectMap, &stack->ownerId);
    const auto* clickedPlayer = clickedStack ? getPlayer(objectMap, &clickedStack->ownerId)
                                             : nullptr;
    const auto* diplomacy = getDiplomacy(objectMap);
    const bool allied = diplomacy && stackPlayer && clickedPlayer && stackPlayer->raceType
                        && stackPlayer->raceType->data && clickedPlayer->raceType
                        && clickedPlayer->raceType->data
                        && CMidDiplomacyApi::get()
                               .areAllies(diplomacy, &stackPlayer->raceType->data->raceType,
                                          &clickedPlayer->raceType->data->raceType);
    const bool visibleActionTarget = clickedStack && !clickedStack->invisible
                                     && clickedStack->ownerId != stack->ownerId
                                     && clickedStack->id != battlePreviewContext.targetId
                                     && !allied;

    WaitingMovementPath planned;
    bool pathLeadsToAction{};
    if (visibleActionTarget) {
        static constexpr std::array<CMqPoint, 8> offsets{{
            {-1, -1},
            {0, -1},
            {1, -1},
            {-1, 0},
            {1, 0},
            {-1, 1},
            {0, 1},
            {1, 1},
        }};

        int bestCost = std::numeric_limits<int>::max();
        for (const auto& offset : offsets) {
            const CMqPoint endpoint = destination + offset;
            auto candidate = planWaitingMovementPath(objectMap, stack, battlePreviewContext.anchor,
                                                     endpoint, ignoredStackId);
            if (candidate.empty()) {
                continue;
            }

            const auto* actionTarget = fn.getBlockingPathNearbyStackId(objectMap, plan, stack,
                                                                       &candidate.back().position,
                                                                       &destination, 0);
            if (!actionTarget
                || (*actionTarget != clickedStack->id
                    && (targetType != IdType::Stack
                        || *actionTarget != battlePreviewContext.targetId))
                || candidate.back().cumulativeCost >= bestCost) {
                continue;
            }

            bestCost = candidate.back().cumulativeCost;
            planned = std::move(candidate);
        }
        pathLeadsToAction = !planned.empty();
    } else {
        planned = planWaitingMovementPath(objectMap, stack, battlePreviewContext.anchor,
                                          destination, ignoredStackId);
    }

    if (planned.empty()) {
        return false;
    }

    const int budget = battlePreviewContext.remainingMovement;
    if (planned.size() > 1 && planned[1].cumulativeCost > budget) {
        return false;
    }

    CMqPoint lastReachable = planned.front().position;
    for (const auto& node : planned) {
        if (node.cumulativeCost <= budget) {
            lastReachable = node.position;
        }
    }

    const bool actionReachable = pathLeadsToAction && planned.back().cumulativeCost < budget;
    if (planned.size() <= 1 && !actionReachable) {
        return false;
    }

    secondSegmentPath.assign(planned.begin() + 1, planned.end());
    if (secondSegmentPath.empty()) {
        secondSegmentPath.push_back(planned.front());
    }

    std::vector<ListNode<WaitingMovementPathNode>> nodes(secondSegmentPath.size());
    ListNode<WaitingMovementPathNode> head{};
    head.next = &nodes.front();
    head.prev = &nodes.back();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        nodes[i].prev = i == 0 ? &head : &nodes[i - 1];
        nodes[i].next = i + 1 == nodes.size() ? &head : &nodes[i + 1];
        nodes[i].data = secondSegmentPath[i];
    }

    List<WaitingMovementPathNode> pathWithCosts{};
    pathWithCosts.length = static_cast<std::uint32_t>(nodes.size());
    pathWithCosts.head = &head;
    auto* path = reinterpret_cast<List<CMqPoint>*>(&pathWithCosts);

    const CMqPoint pathEnd = pathLeadsToAction ? destination : planned.back().position;
    ScopedWaitingMovementSecondSegment secondSegment;
    showMovementPathHooked(objectMap, &stack->id, path, &lastReachable, &pathEnd,
                           pathLeadsToAction);
    spdlog::debug("Waiting movement path preview second segment rendered");
    return true;
}

void stopWaitingPathUpdateEvent()
{
    if (!waitingPathUpdateEventActive) {
        return;
    }

    waitingPathUpdateEventActive = false;
    game::UiEventApi::get().destructor(&waitingPathUpdateEvent);
    waitingPathUpdateEvent = {};
}

bool __fastcall waitingPathUpdateEventCallback(void*, int)
{
    const auto* midgard = game::CMidgardApi::get().instance();
    auto* uiManager = midgard && midgard->data ? midgard->data->uiManager.data : nullptr;
    if (!uiManager || !waitingPath.phase) {
        return false;
    }

    const auto* fn = game::WaitingMovementPathApi::get();
    if (!fn) {
        return false;
    }

    if (!isWaitingPathPhaseCurrent() || !waitingPath.phaseGame || !waitingPath.phaseGame->data) {
        const bool visualChanged = waitingPath.lastUpdateSucceeded;
        destroyWaitingPath(*fn, "phase change");
        if (visualChanged && uiManager->data && uiManager->data->uiKernel) {
            auto* kernel = uiManager->data->uiKernel;
            kernel->vftable->invalidateAndUpdate(kernel);
        }
        return false;
    }

    if (waitingPath.phaseGame->data->clientTakesTurn) {
        if (!waitingPath.opponentTurnObserved) {
            return false;
        }

        const bool visualChanged = waitingPath.lastUpdateSucceeded;
        destroyWaitingPath(*fn, "local turn");
        if (visualChanged && uiManager->data && uiManager->data->uiKernel) {
            auto* kernel = uiManager->data->uiKernel;
            kernel->vftable->invalidateAndUpdate(kernel);
        }
        return false;
    }

    waitingPath.opponentTurnObserved = true;

    const bool leftMouseButtonDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool rightMouseButtonDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    DWORD foregroundProcessId{};
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcessId);
    if (foregroundProcessId != GetCurrentProcessId()) {
        waitingPath.leftMouseButtonDown = leftMouseButtonDown;
        waitingPath.rightMouseButtonDown = rightMouseButtonDown;
        return false;
    }

    const bool leftMouseButtonPressed = leftMouseButtonDown && !waitingPath.leftMouseButtonDown;
    const bool rightMouseButtonPressed = rightMouseButtonDown && !waitingPath.rightMouseButtonDown;
    waitingPath.leftMouseButtonDown = leftMouseButtonDown;
    waitingPath.rightMouseButtonDown = rightMouseButtonDown;
    if (!leftMouseButtonPressed && !rightMouseButtonPressed) {
        return false;
    }

    game::CMqPoint mousePosition{};
    if (game::CUIManagerApi::get().getMousePosition(uiManager, &mousePosition)) {
        bool visualChanged{};
        const bool stackSelected = leftMouseButtonPressed
                                   && trySelectWaitingPreviewStack(&mousePosition, &visualChanged);
        if (!stackSelected) {
            if (leftMouseButtonPressed) {
                visualChanged = updateWaitingPathPreview(&mousePosition);
            } else {
                visualChanged = updateSecondSegmentPreview(&mousePosition);
            }
        }
        if (visualChanged && uiManager->data && uiManager->data->uiKernel) {
            auto* kernel = uiManager->data->uiKernel;
            kernel->vftable->invalidateAndUpdate(kernel);
        }
    }

    return false;
}

bool startWaitingPathUpdateEvent()
{
    if (waitingPathUpdateEventActive) {
        return true;
    }

    using namespace game;

    const auto& uiManagerApi = CUIManagerApi::get();
    CUIManagerApi::Api::UpdateEventCallback callback{};
    callback.callback = (CUIManagerApi::Api::UpdateEventCallback::Callback)
        waitingPathUpdateEventCallback;

    SmartPointer functor{};
    uiManagerApi.createUpdateEventFunctor(&functor, 0, nullptr, &callback);

    UIManagerPtr uiManager{};
    uiManagerApi.get(&uiManager);

    waitingPathUpdateEventActive = uiManager.data && functor.data
                                   && uiManagerApi.createUpdateEvent(uiManager.data,
                                                                     &waitingPathUpdateEvent,
                                                                     &functor);

    SmartPointerApi::get().createOrFreeNoDtor(&functor, nullptr);
    SmartPointerApi::get().createOrFree((SmartPointer*)&uiManager, nullptr);
    return waitingPathUpdateEventActive;
}

void __fastcall taskManagerSetCurrentTaskHooked(game::CTaskManager* thisptr,
                                                int,
                                                game::ITask* nextTask)
{
    const auto* fn = game::WaitingMovementPathApi::get();
    auto* currentTask = thisptr && thisptr->data ? thisptr->data->currentTask : nullptr;

    if (fn && currentTask && currentTask != nextTask
        && currentTask->vftable == fn->taskSelectUnitVftable
        && (!nextTask || nextTask->vftable == fn->taskMsgBoxVftable
            || nextTask->vftable == fn->taskWaitVftable)) {
        savePendingWaitingPath(thisptr, static_cast<CTaskSelectUnit*>(currentTask));
    }

    if (fn && currentTask && currentTask != nextTask
        && currentTask->vftable == fn->taskSelectCityVftable
        && (!nextTask || nextTask->vftable == fn->taskMsgBoxVftable
            || nextTask->vftable == fn->taskWaitVftable)) {
        savePendingSelectedCity(thisptr, currentTask);
    }

    taskManagerSetCurrentTask(thisptr, nextTask);

    if (!fn || !thisptr || !thisptr->data) {
        return;
    }

    auto* actualTask = thisptr->data->currentTask;
    if (actualTask && actualTask->vftable == fn->taskWaitVftable) {
        if (pendingWaitingPath.taskManager != thisptr && !waitingPath.phase) {
            const auto* waitTaskData = getTaskWaitData(actualTask);
            savePendingFortifiedStack(thisptr, waitTaskData ? waitTaskData->phaseGame : nullptr);
        }
        if (pendingWaitingPath.taskManager == thisptr) {
            activateWaitingPath(thisptr, actualTask);
        }
        return;
    }

    if (pendingWaitingPath.taskManager == thisptr && actualTask
        && actualTask->vftable != fn->taskSelectUnitVftable
        && actualTask->vftable != fn->taskSelectCityVftable
        && actualTask->vftable != fn->taskMsgBoxVftable
        && actualTask->vftable != fn->midNullTaskVftable) {
        pendingWaitingPath = {};
    }
}

} // namespace

void addWaitingMovementPathHooks(Hooks& hooks)
{
    const auto* fn = game::WaitingMovementPathApi::get();
    if (!fn) {
        return;
    }

    hooks.emplace_back(HookInfo{game::CTaskManagerApi::get().setCurrentTask,
                                taskManagerSetCurrentTaskHooked,
                                (void**)&taskManagerSetCurrentTask});
    spdlog::info("Waiting movement path preview hooks registered");
}

void stopWaitingMovementPathPreview()
{
    pendingWaitingPath = {};

    if (!waitingPath.phase && !waitingPath.path && !waitingPathUpdateEventActive) {
        return;
    }

    const auto* fn = game::WaitingMovementPathApi::get();
    if (fn) {
        destroyWaitingPath(*fn, "network state cleared");
        return;
    }

    stopWaitingPathUpdateEvent();
    waitingPath = {};
    clearMovementPathImages();
}

bool isWaitingMovementPathPreviewActive()
{
    return waitingPath.phase != nullptr;
}

bool isWaitingMovementPathPreview()
{
    return waitingMovementPathPreview;
}

bool isWaitingMovementPathSecondSegmentPreview()
{
    return waitingMovementPathSecondSegmentPreview;
}

int getWaitingMovementPathSecondSegmentBudget()
{
    return battlePreviewContext.valid ? battlePreviewContext.remainingMovement : 0;
}

bool getWaitingMovementPathSecondSegmentCost(const game::CMqPoint* mapPosition, int* cost)
{
    if (!waitingMovementPathSecondSegmentPreview || !mapPosition || !cost) {
        return false;
    }

    for (const auto& node : secondSegmentPath) {
        if (node.position == *mapPosition) {
            *cost = node.cumulativeCost;
            return true;
        }
    }

    return false;
}

void setWaitingMovementPathBattleContext(const game::CMqPoint* anchor,
                                         const game::CMidgardID* targetId,
                                         const game::CMqPoint* targetPosition,
                                         int remainingMovement,
                                         int maxMovement)
{
    if (!waitingPath.phase || !anchor || !targetId || !targetPosition) {
        battlePreviewContext = {};
        return;
    }

    battlePreviewContext.anchor = *anchor;
    battlePreviewContext.targetId = *targetId;
    battlePreviewContext.targetPosition = *targetPosition;
    battlePreviewContext.remainingMovement = std::max(0, remainingMovement);
    battlePreviewContext.maxMovement = std::max(0, maxMovement);
    battlePreviewContext.valid = true;
}

void clearWaitingMovementPathBattleContext()
{
    battlePreviewContext = {};
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
