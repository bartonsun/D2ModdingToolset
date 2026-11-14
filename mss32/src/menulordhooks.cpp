/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 */

#include "menulordhooks.h"
#include "button.h"
#include "dialoginterf.h"
#include "menubase.h"
#include "menulord.h"
#include "originalfunctions.h"
#include <spdlog/spdlog.h>

namespace hooks {

bool lockLordFaceButton = false;
bool isLoadingSkirmishMultiSave = false;

void resetLordLockState()
{
    if (lockLordFaceButton || isLoadingSkirmishMultiSave) {
        spdlog::debug("resetLordLockState: lockLordFaceButton {} -> false, "
                      "isLoadingSkirmishMultiSave {} -> false",
                      lockLordFaceButton, isLoadingSkirmishMultiSave);
    }

    lockLordFaceButton = false;
    isLoadingSkirmishMultiSave = false;
}

void __fastcall menuLordFaceButtonClickHooked(game::CMenuLord* thisptr, int /*%edx*/)
{
    spdlog::debug("menuLordFaceButtonClickHooked called, lockLordFaceButton = {}", lockLordFaceButton);

    if (lockLordFaceButton) {
        spdlog::debug("Lord face button click ignored (locked after save load)");
        return;
    }

    getOriginalFunctions().menuLordFaceButtonClick(thisptr);
}

game::CMenuLord* __fastcall menuLordCtorHooked(game::CMenuLord* thisptr,
                                               int /*%edx*/,
                                               game::CMenuPhase* menuPhase)
{
    using namespace game;

    spdlog::debug("menuLordCtorHooked called, lockLordFaceButton = {}", lockLordFaceButton);

    getOriginalFunctions().menuLordCtor(thisptr, menuPhase);

    if (lockLordFaceButton) {
        auto dialog = CMenuBaseApi::get().getDialogInterface(thisptr);
        // FIX #22: dialog may be null - never pass it to findButton then.
        auto button = dialog ? CDialogInterfApi::get().findButton(dialog, "BTN_LORD") : nullptr;
        if (button) {
            spdlog::debug("Disabling BTN_LORD button (locked after save load)");
            button->vftable->setEnabled(button, false);
        } else {
            spdlog::debug("BTN_LORD button not found, nothing to disable");
        }

        // Do NOT reset lockLordFaceButton here: it must stay true for the
        // whole loaded-game session (it also gates CMenusReqLordMsg sending
        // in netcustomplayerclient.cpp). It is only cleared when a genuinely
        // new game is started (see MenuPhase::Single2NewSkirmish in menuphasehooks.cpp).
    }

    return thisptr;
}


void __fastcall hotseatLobbyRaceLordButtonClickHooked(void* thisptr, int /*%edx*/)
{
    spdlog::debug("hotseatLobbyRaceLordButtonClickHooked called, lockLordFaceButton = {}", lockLordFaceButton);

    if (lockLordFaceButton) {
        spdlog::debug("Hotseat lobby lord button click ignored (locked after save load)");
        return;
    }

    getOriginalFunctions().hotseatLobbyRaceLordButtonClick(thisptr);
}

void __fastcall lobbyLordButtonClickHooked(void* thisptr, int /*%edx*/)
{
    spdlog::debug("lobbyLordButtonClickHooked called, lockLordFaceButton = {}", lockLordFaceButton);

    if (lockLordFaceButton) {
        spdlog::debug("Lobby lord button click ignored (locked after save load)");
        return;
    }

    getOriginalFunctions().lobbyLordButtonClick(thisptr);
}
} // namespace hooks

