#ifndef TERRITORYCOVERAGEHOOKS_H
#define TERRITORYCOVERAGEHOOKS_H

#include "midgardmapblock.h"

namespace hooks {

bool __fastcall countTerrainCoverageHooked(const game::CMidgardMapBlock* thisptr,
                                           int /*%edx*/,
                                           game::TerrainCountMap* terrainCoverage,
                                           int* plainTiles);

} // namespace hooks

#endif // TERRITORYCOVERAGEHOOKS_H
