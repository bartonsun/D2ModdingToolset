/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2020 Vladimir Makeev.
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

#ifndef VERSION_H
#define VERSION_H

#include <filesystem>
#include <system_error>

namespace hooks {

enum class GameVersion
{
    Unknown = -1,
    Akella,
    Russobit,
    Gog,
    ScenarioEditor
};

/** Returns determined game version. */
GameVersion gameVersion();

/** Returns true if dll loaded from game executable. */
bool executableIsGame();

std::error_code determineGameVersion(const std::filesystem::path& exeFilePath);

} // namespace hooks

#endif // VERSION_H
