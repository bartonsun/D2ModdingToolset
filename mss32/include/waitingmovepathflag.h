#pragma once

namespace game {
struct IMqImage2;
struct GameImages;
}

namespace hooks {

/** Flag of the waiting route: MOVEWAIT from Imgs/IsoCmon.ff or a drawn gray flag. */
game::IMqImage2* getWaitingMovementPathFlag(game::GameImages* images);

}
