/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2023 Stanislav Egorov.
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

#ifndef COMMANDMSGLISTENER_H
#define COMMANDMSGLISTENER_H

#include <optional>
#include <sol/sol.hpp>

namespace game {
struct IMidgardObjectMap;
struct BattleMsgData;
} // namespace game

namespace hooks {

struct CommandMsgListenerFunctions;

class CCommandMsgListener
{
public:
    CCommandMsgListener();
    virtual ~CCommandMsgListener();

    void onBattleStart(const game::IMidgardObjectMap* objectMap,
                       const game::BattleMsgData* battle) const;
    void onBattleChooseAction(const game::IMidgardObjectMap* objectMap,
                              const game::BattleMsgData* battle) const;
    void onBattleResult(const game::IMidgardObjectMap* objectMap,
                        const game::BattleMsgData* battle) const;
    void onBattleEnd(const game::IMidgardObjectMap* objectMap,
                     const game::BattleMsgData* battle) const;

protected:
    void execute(std::optional<sol::function> function,
                 const game::IMidgardObjectMap* objectMap,
                 const game::BattleMsgData* battle) const;
    void showScriptErrorMessage(const char* reason) const;

private:
    const CommandMsgListenerFunctions& getFunctions() const;

    mutable CommandMsgListenerFunctions* mainThreadFunctions;
    mutable CommandMsgListenerFunctions* workerThreadFunctions;
};

CCommandMsgListener& getCommandMsgListener();

} // namespace hooks

#endif // COMMANDMSGLISTENER_H
