/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 */
#ifndef MENULORDHOOKS_H
#define MENULORDHOOKS_H

namespace game {
struct CMenuLord;
struct CMenuPhase;
} // namespace game

namespace hooks {

/**
 * When true, blocks lord/race change on CMenuLord, DLG_LOBBY and the hotseat
 * lobby race screen (clicks are ignored, BTN_LORD is disabled where present).
 * Used to prevent changing the lord after loading a saved game.
 * It is NOT cleared when a CMenuLord screen closes - it stays set for the whole
 * loaded-save session. It is cleared by resetLordLockState() whenever we get back
 * to the main menu / custom lobby, and when a brand new game is started.
 */
extern bool lockLordFaceButton;

/**
 * Set to true right when a saved skirmish game genuinely begins loading
 * through the network/multiplayer load path (MenuPhase::Single2LoadSkirmish,
 * networkGame branch). Used to disambiguate MenuPhase::NewSkirmish2LobbyHost
 * from MenuPhase::LoadSkirmishMulti, which share the same underlying value,
 * instead of guessing after the fact from a possibly stale currentMenu.
 * Reset to false immediately after being checked.
 */
extern bool isLoadingSkirmishMultiSave;

/**
 * FIX #22: clears BOTH lockLordFaceButton and isLoadingSkirmishMultiSave.
 * Call it at every point where no loaded-save session can be in progress
 * (back to main menu / custom lobby, start of a brand new game, failed join),
 * so stale state can neither lock a new game nor block a campaign lord screen.
 */
void resetLordLockState();

void __fastcall menuLordFaceButtonClickHooked(game::CMenuLord* thisptr, int /*%edx*/);

/**
 * Disables BTN_LORD button (used in DLG_LOBBY) right after CMenuLord is constructed,
 * if lockLordFaceButton is set. Makes the button visually disabled (greyed out,
 * no hover highlight) instead of just ignoring clicks. Resets lockLordFaceButton
 * back to false afterwards.
 */
game::CMenuLord* __fastcall menuLordCtorHooked(game::CMenuLord* thisptr,
                                               int /*%edx*/,
                                               game::CMenuPhase* menuPhase);


/**
 * Hook for the lord type button click handler used on the hotseat lobby
 * race/lord selection screen (DLG_HOTSEAT_LOBBY_RACE), a separate screen
 * from CMenuLord with its own click handler at address 0x4ddf29.
 * Blocks the click the same way as menuLordFaceButtonClickHooked when
 * lockLordFaceButton is set.
 */
void __fastcall hotseatLobbyRaceLordButtonClickHooked(void* thisptr, int /*%edx*/);

/**
 * Hook for the lord type button click handler used on the regular network
 * lobby screen (DLG_LOBBY), a separate screen from CMenuLord and from the
 * hotseat lobby race screen, with its own click handler at address 0x4e35de.
 * Blocks the click the same way as menuLordFaceButtonClickHooked when
 * lockLordFaceButton is set.
 */
void __fastcall lobbyLordButtonClickHooked(void* thisptr, int /*%edx*/);
} // namespace hooks

#endif // MENULORDHOOKS_H

