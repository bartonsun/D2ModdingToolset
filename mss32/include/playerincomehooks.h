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

#ifndef PLAYERINCOMEHOOKS_H
#define PLAYERINCOMEHOOKS_H

#include <vector>

namespace game {
struct Bank;
struct IMidgardObjectMap;
struct CMidgardID;
struct CMidServerLogic;
} // namespace game

namespace hooks {

struct HookInfo;

game::Bank* __stdcall computePlayerDailyIncomeHooked(game::Bank* income,
                                                     game::IMidgardObjectMap* objectMap,
                                                     const game::CMidgardID* playerId);

/** Starting resources, command replay and computer bonus hooks of network games. */
void addNetworkIncomeHooks(std::vector<HookInfo>& hooks);

void resetNetworkIncome();
void rememberHumanPlayers(const game::CMidServerLogic* logic);
void observeStartingResources(const game::CMidServerLogic* logic, int turn);
bool isIncomeCredited(const game::CMidServerLogic* logic, const game::CMidgardID* playerId);
bool claimTurnStart(const game::CMidServerLogic* logic, const game::CMidgardID* playerId);

/** Server that runs BeginTurn, before CMidgard publishes it. */
class TurnIncomeScope
{
public:
    explicit TurnIncomeScope(const game::CMidServerLogic* logic);
    ~TurnIncomeScope();

    TurnIncomeScope(const TurnIncomeScope&) = delete;
    TurnIncomeScope& operator=(const TurnIncomeScope&) = delete;

private:
    const game::CMidServerLogic* previous;
};

/** Network game restored after its first day: the day's income is already paid. */
class RestoredGameScope
{
public:
    RestoredGameScope(game::CMidServerLogic* logic, bool currentPlayerReady);
    ~RestoredGameScope();

    RestoredGameScope(const RestoredGameScope&) = delete;
    RestoredGameScope& operator=(const RestoredGameScope&) = delete;

private:
    game::CMidServerLogic* previous;
    int previousTurn;
    bool previousMarked;
};

} // namespace hooks

#endif // PLAYERINCOMEHOOKS_H
