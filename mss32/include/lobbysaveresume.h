/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/bartonsun/D2ModdingToolset)
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LOBBYSAVERESUME_H
#define LOBBYSAVERESUME_H

namespace game {
struct CMidgardScenarioMap;
struct CMidStreamEnvFile;
struct CMidServerLogic;
} // namespace game

namespace hooks {

/** Retains the legacy save's turn-order origin before the load menu changes host race. */
void captureLobbySaveTurnBase(game::CMidgardScenarioMap* scenarioMap,
                              const game::CMidStreamEnvFile* streamEnv);

/** Restores the full native queue, including AI. Call before loaded-game initialization. */
bool prepareLobbySaveResume(game::CMidServerLogic* logic);

} // namespace hooks

#endif // LOBBYSAVERESUME_H
