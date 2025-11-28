#include "landmarkview.h"
#include "landmarkutils.h"
#include "midlandmark.h"
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace bindings {

LandmarkView::LandmarkView(const game::CMidLandmark* landmark,
                           const game::IMidgardObjectMap* objectMap)
    : landmark{landmark}
    , objectMap{objectMap}
{ }

void LandmarkView::bind(sol::state& lua)
{
    auto landmarkView = lua.new_usertype<LandmarkView>("LandmarkView");
    landmarkView["id"] = sol::property(&LandmarkView::getId);
    landmarkView["position"] = sol::property(&LandmarkView::getPosition);
    landmarkView["size"] = sol::property(&LandmarkView::getSize);
    landmarkView["description"] = sol::property(&LandmarkView::getDescription);
    landmarkView["typeId"] = sol::property(&LandmarkView::getTypeId);
    landmarkView["mountain"] = sol::property(&LandmarkView::isMountain);
}

IdView LandmarkView::getId() const
{
    return IdView{landmark->id};
}

Point LandmarkView::getPosition() const
{
    return Point{landmark->mapElement.position};
}

Point LandmarkView::getSize() const
{
    return Point{landmark->mapElement.sizeX, landmark->mapElement.sizeY};
}

std::string LandmarkView::getDescription() const
{
    return landmark->description.string;
}

IdView LandmarkView::getTypeId() const
{
    return IdView{landmark->landmarkTypeId};
}

bool LandmarkView::isMountain() const
{
    if (!landmark) {
        return false;
    }

    return hooks::isLandmarkMountain(&landmark->landmarkTypeId);
}

} // namespace bindings