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

#ifndef WAITINGMOVEPATHHOOKS_H
#define WAITINGMOVEPATHHOOKS_H

#include "hooks.h"

namespace game {
struct CMqPoint;
struct IMidgardObjectMap;
} // namespace game

namespace hooks {

/** Adds detours used to retain a local path calculator for the waiting task. */
void addWaitingMovementPathHooks(Hooks& hooks);

/** Adds the CTaskWait mouse handler override. */
void addWaitingMovementPathVftableHooks(Hooks& hooks);

/** True only while a waiting-task route is being calculated. */
bool isWaitingMovementPathPreview();

/** False for fogged tiles while the waiting-task route is being calculated. */
bool isWaitingMovementPathPreviewTileVisible(const game::IMidgardObjectMap* objectMap,
                                             const game::CMqPoint* mapPosition);

/** False when a waiting-task water route would inspect fogged neighbouring tiles. */
bool isWaitingMovementPathPreviewAreaVisible(const game::IMidgardObjectMap* objectMap,
                                             const game::CMqPoint* mapPosition);

} // namespace hooks

#endif // WAITINGMOVEPATHHOOKS_H
