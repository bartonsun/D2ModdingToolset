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

#ifndef MIDCONDPLAYERTYPE_H
#define MIDCONDPLAYERTYPE_H

namespace game {
struct CMidEvCondition;
struct String;
struct IMidgardObjectMap;
struct LEventCondCategory;
struct ITask;
struct CMidgardID;
struct ITestCondition;
struct CDialogInterf;

namespace editor {
struct CCondInterf;
}

} // namespace game

namespace hooks {

game::CMidEvCondition* createMidCondPlayerType();

void __stdcall midCondPlayerTypeGetInfoString(game::String* info,
                                              const game::IMidgardObjectMap* objectMap,
                                              const game::CMidEvCondition* eventCondition);

game::editor::CCondInterf* createCondPlayerTypeInterf(game::ITask* task,
                                                      void* a2,
                                                      const game::CMidgardID* eventId);

game::ITestCondition* createTestPlayerType(game::CMidEvCondition* eventCondition);

bool checkPlayerTypeConditionValid(game::CDialogInterf* dialog,
                                   const game::IMidgardObjectMap* objectMap,
                                   const game::CMidgardID* eventId);

} // namespace hooks

#endif // MIDCONDPLAYERTYPE_H
