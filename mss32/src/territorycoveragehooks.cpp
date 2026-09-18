#include "territorycoveragehooks.h"
#include "categoryids.h"
#include "midgardmapblock.h"
#include "terraincat.h"
#include "terraincountmap.h"

namespace hooks {

bool __fastcall countTerrainCoverageHooked(const game::CMidgardMapBlock* thisptr,
                                           int /*%edx*/,
                                           game::TerrainCountMap* terrainCoverage,
                                           int* plainTiles)
{
    using namespace game;

    if (!thisptr || !terrainCoverage || !plainTiles) {
        return false;
    }

    int human = 0;
    int dwarf = 0;
    int heretic = 0;
    int undead = 0;
    int neutral = 0;
    int elf = 0;
    int counted = 0;

    for (int i = 0; i < 32; ++i) {
        const std::uint32_t tile = thisptr->tiles[i];
        const GroundId ground = tileGround(tile);
        if (ground != GroundId::Plain && ground != GroundId::Forest) {
            continue;
        }

        ++counted;
        switch (tileTerrain(tile)) {
        case TerrainId::Human:
            ++human;
            break;
        case TerrainId::Dwarf:
            ++dwarf;
            break;
        case TerrainId::Heretic:
            ++heretic;
            break;
        case TerrainId::Undead:
            ++undead;
            break;
        case TerrainId::Elf:
            ++elf;
            break;
        default:
            ++neutral;
            break;
        }
    }

    const auto& terrains = TerrainCategories::get();
    const auto access = TerrainCountMapApi::get().access;

    *access(terrainCoverage, terrains.human) += human;
    *access(terrainCoverage, terrains.dwarf) += dwarf;
    *access(terrainCoverage, terrains.heretic) += heretic;
    *access(terrainCoverage, terrains.undead) += undead;
    *access(terrainCoverage, terrains.neutral) += neutral;
    *access(terrainCoverage, terrains.elf) += elf;
    *plainTiles += counted;

    return true;
}

} // namespace hooks
