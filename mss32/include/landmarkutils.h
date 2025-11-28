#ifndef LANDMARKUTILS_H
#define LANDMARKUTILS_H

namespace game {
struct TLandmarkData;
struct CMidgardID;
} // namespace game

namespace hooks {

bool isLandmarkMountain(const game::CMidgardID* landmarkTypeId);

} // namespace hooks

#endif // LANDMARKUTILS_H