/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/bartonsun/D2ModdingToolset)
 * Copyright (C) 2026 Max Vynogradov.
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

#include "quicksavehook.h"
// TODO: If more strategic interface hotkeys are added, rename this file to better reflect its broader purpose.
#include "game.h"

#include <Windows.h>
#include <settings.h>
using namespace game;

namespace hooks {
using KeyHandler = int(__thiscall*)(void* thisPtr, int key, int a3);

KeyHandler originalKeyHandler = nullptr;

constexpr const char* QuickSaveName = "QuickSave";

bool isHotkeyPressed(const Settings::Hotkey& hotkey, int key)
{
    if (static_cast<std::uint32_t>(std::toupper(static_cast<unsigned char>(key))) != hotkey.key) {
        return false;
    }

    if (hotkey.ctrl != ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0))
        return false;

    if (hotkey.shift != ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0))
        return false;

    if (hotkey.alt != ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0))
        return false;

    return true;
}

int __fastcall hookedKeyHandler(void* thisPtr, void*, int key, int a3)
{
    auto realThis = reinterpret_cast<char*>(thisPtr) - 4;

    if (isHotkeyPressed(userSettings().hotkeys.quickSave, key)) {
        gameFunctions().sendSaveGameMsgToServer(realThis, QuickSaveName);
        return 1;
    }


    if (isHotkeyPressed(userSettings().hotkeys.openSelectedObject, key)) {
        gameFunctions().stratInterfOpenSelectedObject(realThis);
        return 1;
    }

    return originalKeyHandler(thisPtr, key, a3);
}
} // namespace
