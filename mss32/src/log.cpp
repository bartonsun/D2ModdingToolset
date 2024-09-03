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

#include "log.h"
#include "settings.h"
#include "utils.h"
#include <Windows.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

extern std::thread::id mainThreadId;

namespace hooks {

static void logAction(std::string_view logFile, std::string_view message)
{
    // TODO: replace this shit with multithreaded spdlog
    auto path{hooks::gameFolder() / logFile};
    if (std::this_thread::get_id() != mainThreadId) {
        path.replace_extension(".server.log");
    }

    std::ofstream file(path.c_str(), std::ios_base::app);
    const std::time_t time{std::time(nullptr)};
    const std::tm tm = *std::localtime(&time);
    file << "[" << std::put_time(&tm, "%c") << "]\t" << message << "\n";
}

void logDebug(std::string_view logFile, std::string_view message)
{
#ifdef _DEBUG
    std::stringstream msg;
    msg << "[" << logFile << "]\t" << std::this_thread::get_id() << "\t" << message << "\n";
    OutputDebugString(msg.str().c_str());
#endif

    if (userSettings().debugMode) {
        logAction(logFile, message);
    }
}

void logError(std::string_view logFile, std::string_view message)
{
    logAction(logFile, message);
}

} // namespace hooks
