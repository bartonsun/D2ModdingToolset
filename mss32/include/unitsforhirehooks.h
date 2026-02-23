#ifndef UNITSTOHIREHOOKS_H
#define UNITSTOHIREHOOKS_H

#include "idlist.h"

namespace game {
struct CMidDataCache2;
struct CMidgardID;
struct CMidPlayer;
struct CPhaseGame;
} // namespace game

namespace hooks {

bool __stdcall addPlayerUnitsToHireListHooked(game::CMidDataCache2* dataCache,
                                              const game::CMidgardID* playerId,
                                              const game::CMidgardID* fortificationId,
                                              game::IdList* hireList);

bool __stdcall shouldAddUnitToHireHooked(const game::CMidPlayer* player,
                                         game::CPhaseGame* phaseGame,
                                         const game::CMidgardID* unitImplId);

bool __stdcall enableUnitInHireListUiHooked(const game::CMidPlayer* player,
                                            game::CPhaseGame* phaseGame,
                                            const game::CMidgardID* unitImplId);

void __stdcall addSideshowUnitToUIHooked(game::IdList* unitIds,
                                         game::CMidPlayer* player,
                                         game::CPhaseGame* phaseGame);

} // namespace hooks

#endif // UNITSTOHIREHOOKS_H