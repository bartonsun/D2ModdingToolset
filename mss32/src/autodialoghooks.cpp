/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Stanislav Egorov.
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

#include "autodialoghooks.h"
#include "..\resource.h"
#include "autodialog.h"
#include "autodialogfile.h"
#include "d2string.h"
#include "menucustomlobby.h"
#include "menucustomprotocol.h"
#include "originalfunctions.h"
#include "stringarray.h"

extern HMODULE library;

namespace hooks {

bool addDefaultDialog(game::CAutoDialog* autoDialog, int resourceId)
{
    using namespace game;

    const auto& stringApi = StringApi::get();
    const auto& stringArrayApi = StringArrayApi::get();
    const auto& autoDialogApi = AutoDialogApi::get();
    const auto& autoDialogFileApi = AutoDialogFileApi::get();

    HRSRC resourceInfo = ::FindResource(library, MAKEINTRESOURCE(resourceId), "TEXT");
    if (resourceInfo == NULL) {
        return false;
    }

    DWORD resourceSize = ::SizeofResource(library, resourceInfo);
    if (resourceSize == 0) {
        return false;
    }

    HGLOBAL resource = ::LoadResource(library, resourceInfo);
    if (resource == NULL) {
        return false;
    }

    CAutoDialogFile file{};
    file.readMode = true;
    auto data = reinterpret_cast<const char*>(::LockResource(resource));
    auto begin = data;
    for (DWORD i = 0; i < resourceSize; ++i) {
        auto ptr = data + i;
        if (*ptr != '\n') {
            continue;
        }

        size_t length = ptr - begin;
        if (length && *(ptr - 1) == '\r') {
            --length;
        }

        if (length) {
            String text{};
            stringApi.initFromStringN(&text, begin, length);
            stringArrayApi.pushBack(&file.lines, &text);
            stringApi.free(&text);
        }

        begin = ptr + 1;
    }

    DialogDescriptor* descriptor = nullptr;
    if (!autoDialogApi.parseDialogFromScriptFile(&descriptor, autoDialog, &file)) {
        autoDialogFileApi.destructor(&file);
        return false;
    }

    Pair<DialogMapIterator, bool> result{};
    DialogMapValue entry = {};
    strncpy(entry.first, descriptor->name, sizeof(entry.first));
    entry.second = descriptor;
    if (!autoDialogApi.dialogMapInsert(&autoDialog->data->dialogs, &result, &entry)->second) {
        autoDialogFileApi.destructor(&file);
        return false;
    }

    autoDialogFileApi.destructor(&file);
    return true;
}

bool __fastcall autoDialogLoadAndParseScriptFileHooked(game::CAutoDialog* thisptr,
                                                       int /*%edx*/,
                                                       const char* filePath)
{
    bool result = getOriginalFunctions().autoDialogLoadAndParseScriptFile(thisptr, filePath);
    if (!result) {
        return false;
    }

    bool isCustomLobbyFound = false;
    bool isRoomPasswordFound = false;
    bool isLoginAccountFound = false;
    bool isRegisterAccountFound = false;
    auto& dialogs = thisptr->data->dialogs;
    for (const auto& dialog : dialogs) {
        if (!isCustomLobbyFound && !strcmp(dialog.first, CMenuCustomLobby::dialogName)) {
            isCustomLobbyFound = true;
        } else if (!isRoomPasswordFound
                   && !strcmp(dialog.first, CMenuCustomLobby::roomPasswordDialogName)) {
            isRoomPasswordFound = true;
        } else if (!isLoginAccountFound
                   && !strcmp(dialog.first, CMenuCustomProtocol::loginAccountDialogName)) {
            isLoginAccountFound = true;
        } else if (!isRegisterAccountFound
                   && !strcmp(dialog.first, CMenuCustomProtocol::registerAccountDialogName)) {
            isRegisterAccountFound = true;
        }
    }

    if (!isCustomLobbyFound && !addDefaultDialog(thisptr, DLG_CUSTOM_LOBBY)) {
        return false;
    }

    if (!isRoomPasswordFound && !addDefaultDialog(thisptr, DLG_ROOM_PASSWORD)) {
        return false;
    }

    if (!isLoginAccountFound && !addDefaultDialog(thisptr, DLG_LOGIN_ACCOUNT)) {
        return false;
    }

    if (!isRegisterAccountFound && !addDefaultDialog(thisptr, DLG_REGISTER_ACCOUNT)) {
        return false;
    }

    return true;
}

} // namespace hooks
