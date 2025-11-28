#ifndef LANDMARKVIEW_H
#define LANDMARKVIEW_H

#include "idview.h"
#include "point.h"
#include <string>

namespace sol {
class state;
}

namespace game {
struct CMidLandmark;
struct IMidgardObjectMap;
struct TLandmarkData;
struct CMidgardID;
} // namespace game

namespace bindings {

class LandmarkView
{
public:
    LandmarkView(const game::CMidLandmark* landmark, const game::IMidgardObjectMap* objectMap);

    static void bind(sol::state& lua);

    IdView getId() const;
    Point getPosition() const;
    Point getSize() const;
    std::string getDescription() const;
    IdView getTypeId() const;
    bool isMountain() const;

private:
    const game::CMidLandmark* landmark;
    const game::IMidgardObjectMap* objectMap;
};

} // namespace bindings

#endif // LANDMARKVIEW_H