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

#ifndef AUTODIALOG_H
#define AUTODIALOG_H

#include "d2assert.h"
#include "d2map.h"
#include "d2string.h"
#include "mqrect.h"
#include "smartptr.h"
#include "stringarray.h"

namespace game {

struct CAutoDialog;
struct IMqImage2;
struct CMidAutoDlgImages;
struct CMidAutoDlgLog;
struct CMidAutoDlgTextLoader;
struct DialogDescriptor;
struct CAutoDialogFile;

struct DialogDescriptor
{
    CAutoDialog* autoDialog;
    char name[48];
    CMqRect rect;
    CMqRect clipRect;
    char backgroundName[48];
    bool isAutoBackground;
    char padding[3];
    int cursorType;
    int unknown140; // Always 0
    char cursorName[48];
    char childs[36]; // Unknown key-value container
};

assert_size(DialogDescriptor, 228);
assert_offset(DialogDescriptor, unknown140, 140);

using DialogMap = Map<char[48], DialogDescriptor*>;
using DialogMapIterator = MapIterator<char[48], DialogDescriptor*>;
using DialogMapValue = Pair<char[48], DialogDescriptor*>;

struct CAutoDialogData
{
    SmartPtr<CMidAutoDlgImages> images;
    SmartPtr<CMidAutoDlgLog> log;
    SmartPtr<CMidAutoDlgTextLoader> textLoader;
    SmartPointer memPool;
    DialogMap dialogs; // Map by dialog name
};

assert_size(CAutoDialogData, 60);

/** Holds necessary data to create CInterface objects from .dlg files. */
struct CAutoDialog
{
    CAutoDialogData* data;
};

assert_size(CAutoDialog, 4);

namespace AutoDialogApi {

struct Api
{
    using LoadAndParseScriptFile = bool(__thiscall*)(CAutoDialog* thisptr, const char* filePath);
    LoadAndParseScriptFile loadAndParseScriptFile;

    /** Parses a single dialog from the script file, line by line incrementing currentLineIdx. */
    using ParseDialogFromScriptFile = bool(__stdcall*)(DialogDescriptor** result,
                                                       CAutoDialog* dialog,
                                                       CAutoDialogFile* scriptFile);
    ParseDialogFromScriptFile parseDialogFromScriptFile;

    using DialogMapInsert =
        Pair<DialogMapIterator, bool>*(__thiscall*)(DialogMap* thisptr,
                                                    Pair<DialogMapIterator, bool>* result,
                                                    DialogMapValue* value);
    DialogMapInsert dialogMapInsert;

    using LoadImage = IMqImage2*(__stdcall*)(const char* imageName);
    LoadImage loadImage;
};

/** Returns AutoDialog functions according to determined version of the game. */
Api& get();

} // namespace AutoDialogApi

} // namespace game

#endif // AUTODIALOG_H
