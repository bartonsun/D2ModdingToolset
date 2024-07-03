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

#include "menucustomrandomscenariomulti.h"
#include "interfaceutils.h"
#include "log.h"
#include "mempool.h"
#include "originalfunctions.h"
#include "textids.h"
#include "utils.h"
#include <fmt/format.h>

namespace hooks {

game::RttiInfo<game::CMenuBaseVftable> CMenuCustomRandomScenarioMulti::rttiInfo = {};

CMenuCustomRandomScenarioMulti::CMenuCustomRandomScenarioMulti(game::CMenuPhase* menuPhase)
    : CMenuRandomScenarioMulti{menuPhase}
    , CMenuCustomBase{this}
    , m_roomsCallback{this}
{
    using namespace game;

    if (rttiInfo.locator == nullptr) {
        replaceRttiInfo(rttiInfo, (CMenuBaseVftable*)this->vftable);
        rttiInfo.vftable.destructor = (CInterfaceVftable::Destructor)&destructor;
    }
    this->vftable = &rttiInfo.vftable;

    startScenario = (StartScenario)createRoomAndServer;
    setAccountNameToEditName();

    getNetService()->addRoomsCallback(&m_roomsCallback);
}

CMenuCustomRandomScenarioMulti::~CMenuCustomRandomScenarioMulti()
{
    using namespace game;

    auto service = getNetService();
    if (service) {
        service->removeRoomsCallback(&m_roomsCallback);
    }
}

void CMenuCustomRandomScenarioMulti::createRoomAndServer(CMenuCustomRandomScenarioMulti* menu)
{
    using namespace game;

    prepareToStartRandomScenario(menu, true);

    auto dialog = CMenuBaseApi::get().getDialogInterface(menu);
    if (!getNetService()->createRoom(getEditBoxText(dialog, "EDIT_GAME"),
                                     getEditBoxText(dialog, "EDIT_PASSWORD"))) {
        logDebug("lobby.log", "Failed to request room creation");
        auto msg{getInterfaceText(textIds().lobby.createRoomRequestFailed.c_str())};
        if (msg.empty()) {
            msg = "Could not request to create a room from the lobby server.";
        }
        showMessageBox(msg);
        return;
    }

    menu->showWaitDialog();
}

void __fastcall CMenuCustomRandomScenarioMulti::destructor(CMenuCustomRandomScenarioMulti* thisptr,
                                                           int /*%edx*/,
                                                           char flags)
{
    thisptr->~CMenuCustomRandomScenarioMulti();

    if (flags & 1) {
        logDebug("transitions.log", "Free CMenuCustomRandomScenarioMulti memory");
        game::Memory::get().freeNonZero(thisptr);
    }
}

void CMenuCustomRandomScenarioMulti::RoomsCallback::CreateRoom_Callback(
    const SLNet::SystemAddress& senderAddress,
    SLNet::CreateRoom_Func* callResult)
{
    using namespace game;

    m_menu->hideWaitDialog();

    switch (auto resultCode = callResult->resultCode) {
    case SLNet::REC_SUCCESS: {
        // Setup game host: reuse original game logic that creates player server and client
        if (CMenuNewSkirmishMultiApi::get().createServer(m_menu)) {
            CMenuPhaseApi::get().switchPhase(m_menu->menuBaseData->menuPhase,
                                             MenuTransition::RandomScenarioMulti2LobbyHost);
        }
        break;
    }

    default: {
        auto msg{getInterfaceText(textIds().lobby.createRoomFailed.c_str())};
        if (msg.empty()) {
            msg = "Could not create a room.\n%ERROR%";
        }
        replace(msg, "%ERROR%", SLNet::RoomsErrorCodeDescription::ToEnglish(resultCode));
        showMessageBox(msg);
        break;
    }
    }
}

} // namespace hooks
