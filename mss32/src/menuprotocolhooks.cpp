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

#include "menuprotocolhooks.h"
#include "dialoginterf.h"
#include "listbox.h"
#include "mempool.h"
#include "menucustomprotocol.h"
#include "menuphase.h"
#include "menuphasehooks.h"
#include "netcustomservice.h"
#include "originalfunctions.h"
#include "textids.h"
#include "uievent.h"
#include "utils.h"

namespace hooks {

void __fastcall menuProtocolDisplayCallbackHooked(game::CMenuProtocol* thisptr,
                                                  int /*%edx*/,
                                                  game::String* string,
                                                  bool a3,
                                                  int selectedIndex)
{
    using namespace game;

    auto dialog = CMenuBaseApi::get().getDialogInterface(thisptr);
    auto listBox = CDialogInterfApi::get().findListBox(dialog, "TLBOX_PROTOCOL");
    int lastIndex = -1;

    if (listBox) {
        lastIndex = listBox->listBoxData->elementsTotal - 1;
    }

    if (selectedIndex == lastIndex) {
        auto serverName{getInterfaceText(textIds().lobby.serverName.c_str())};
        if (serverName.empty()) {
            serverName = "Lobby server";
        }

        StringApi::get().initFromString(string, serverName.c_str());
        return;
    }

    getOriginalFunctions().menuProtocolDisplayCallback(thisptr, string, a3, selectedIndex);
}

void __fastcall menuProtocolContinueHandlerHooked(CMenuCustomProtocol* thisptr, int /*%edx*/)
{
    using namespace game;

    auto dialog = CMenuBaseApi::get().getDialogInterface(thisptr);
    auto listBox = CDialogInterfApi::get().findListBox(dialog, "TLBOX_PROTOCOL");
    if (listBox) {
        auto data = listBox->listBoxData;
        if (data->selectedElement == data->elementsTotal - 1) {
            thisptr->createCustomServiceStartWaitingConnection();
            return;
        }
    }

    getOriginalFunctions().menuProtocolContinueHandler(thisptr);
}

game::CMenuProtocol* __stdcall menuProtocolCreateMenuHooked(game::CMenuPhase* menuPhase)
{
    using namespace game;

    auto menu = (CMenuCustomProtocol*)Memory::get().allocate(sizeof(CMenuCustomProtocol));
    new (menu) CMenuCustomProtocol(menuPhase);

    return menu;
}

} // namespace hooks
