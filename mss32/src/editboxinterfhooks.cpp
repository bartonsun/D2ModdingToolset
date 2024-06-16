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

#include "editboxinterfhooks.h"
#include "originalfunctions.h"
#include "textboxinterf.h"
#include <string>

namespace hooks {

void __fastcall editBoxInterfUpdateHooked(game::CEditBoxInterf* thisptr, int /*%edx*/)
{
    using namespace game;

    getOriginalFunctions().editBoxInterfUpdate(thisptr);

    const auto& data = thisptr->data->editBoxData;
    if (data.patched.isPassword) {
        auto textBox = (CTextBoxInterf*)
                           thisptr->vftable->getChild(thisptr, &thisptr->data->textBoxChildIndex);
        if (textBox) {
            std::string mask(data.inputString.length, '*');
            CTextBoxInterfApi::get().setString(textBox, mask.c_str());
            // TODO: recalc textCursorPos using CFormattedTextImpl
        }
    }
}

game::EditBoxData* __fastcall editBoxDataCtorHooked(game::EditBoxData* thisptr, int /*%edx*/)
{
    getOriginalFunctions().editBoxDataCtor(thisptr);
    thisptr->patched.isPassword = false;
    return thisptr;
}

} // namespace hooks
