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

#include "commandmsglistener.h"
#include "battlemsgdataview.h"
#include "commandmsglistenerfunctions.h"
#include "utils.h"
#include <fmt/format.h>
#include <thread>

extern std::thread::id mainThreadId;

namespace hooks {

static const char* SCRIPT_FILE_NAME = "commandMsgListener.lua";

CCommandMsgListener::CCommandMsgListener()
    : mainThreadFunctions(nullptr)
    , workerThreadFunctions(nullptr)
{ }

CCommandMsgListener ::~CCommandMsgListener()
{
    delete mainThreadFunctions;
    delete workerThreadFunctions;
}

void CCommandMsgListener::onBattleStart(const game::IMidgardObjectMap* objectMap,
                                        const game::BattleMsgData* battle) const
{
    execute(getFunctions().onBattleStart, objectMap, battle);
}

void CCommandMsgListener::onBattleChooseAction(const game::IMidgardObjectMap* objectMap,
                                               const game::BattleMsgData* battle) const
{
    execute(getFunctions().onBattleChooseAction, objectMap, battle);
}

void CCommandMsgListener::onBattleResult(const game::IMidgardObjectMap* objectMap,
                                         const game::BattleMsgData* battle) const
{
    execute(getFunctions().onBattleResult, objectMap, battle);
}

void CCommandMsgListener::onBattleEnd(const game::IMidgardObjectMap* objectMap,
                                      const game::BattleMsgData* battle) const
{
    execute(getFunctions().onBattleEnd, objectMap, battle);
}

void CCommandMsgListener::execute(std::optional<sol::function> function,
                                  const game::IMidgardObjectMap* objectMap,
                                  const game::BattleMsgData* battle) const
{
    try {
        if (function) {
            bindings::BattleMsgDataView battleView{battle, objectMap};
            (*function)(battleView);
        }
    } catch (const std::exception& e) {
        showScriptErrorMessage(e.what());
    }
}

void CCommandMsgListener::showScriptErrorMessage(const char* reason) const
{
    showErrorMessageBox(fmt::format("Failed to run '{:s}' script.\n"
                                    "Reason: '{:s}'",
                                    SCRIPT_FILE_NAME, reason));
}

const CommandMsgListenerFunctions& CCommandMsgListener::getFunctions() const
{
    auto& functions = std::this_thread::get_id() == mainThreadId ? mainThreadFunctions
                                                                 : workerThreadFunctions;
    if (functions == nullptr) {
        // Functions need to be initialized in a corresponding thread to use its own Lua instance
        functions = new CommandMsgListenerFunctions(SCRIPT_FILE_NAME);
    }

    return *functions;
}

CCommandMsgListener& getCommandMsgListener()
{
    static CCommandMsgListener value{};

    return value;
}

} // namespace hooks
