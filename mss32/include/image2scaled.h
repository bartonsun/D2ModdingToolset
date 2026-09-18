#ifndef IMAGE2SCALED_H
#define IMAGE2SCALED_H

namespace game {
struct IMqImage2;
struct CMqPoint;
}

namespace hooks {

/** Creates a static, resized copy of a native portrait including its border.
 * The caller retains ownership of source. Returns nullptr if capture is unsupported.
 * Unlike IMqImage2::render's size parameter, this changes pixels, not the crop.
 */
game::IMqImage2* createScaledImage(game::IMqImage2* source,
                                 const game::CMqPoint& targetSize);

} // namespace hooks

#endif // IMAGE2SCALED_H
