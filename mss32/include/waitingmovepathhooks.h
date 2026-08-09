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

void addWaitingMovementPathHooks(Hooks& hooks);

void addWaitingMovementPathVftableHooks(Hooks& hooks);

bool isWaitingMovementPathPreview();

bool isWaitingMovementPathPreviewTileVisible(const game::IMidgardObjectMap* objectMap,
                                             const game::CMqPoint* mapPosition);

bool isWaitingMovementPathPreviewAreaVisible(const game::IMidgardObjectMap* objectMap,
                                             const game::CMqPoint* mapPosition);

} // namespace hooks

#endif // WAITINGMOVEPATHHOOKS_H
