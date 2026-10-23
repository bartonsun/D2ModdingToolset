#include "waitingmovepathflag.h"
#include "gameimages.h"
#include "image2fill.h"
#include "image2outline.h"
#include "mempool.h"
#include "multilayerimg.h"

#include <cstdint>

namespace hooks {
namespace {

void addFill(game::CMultiLayerImg* image,
             std::uint32_t width,
             std::uint32_t height,
             int x,
             int y,
             const game::Color& color)
{
    using namespace game;

    auto* fill = static_cast<CImage2Fill*>(Memory::get().allocate(sizeof(CImage2Fill)));
    if (!fill) {
        return;
    }

    const auto& fillApi = CImage2FillApi::get();
    fillApi.constructor(fill, width, height);
    fillApi.setColor(fill, &color);
    CMultiLayerImgApi::get().addImage(image, fill, x + 35, y + 8);
}

// Gray flag drawn from rectangles, for IsoCmon.ff without the MOVEWAIT image.
game::IMqImage2* createGrayFlag()
{
    using namespace game;

    auto* result = static_cast<CMultiLayerImg*>(Memory::get().allocate(sizeof(CMultiLayerImg)));
    if (!result) {
        return nullptr;
    }

    const auto& multilayerApi = CMultiLayerImgApi::get();
    multilayerApi.constructor(result);

    auto* spacer = static_cast<CImage2Outline*>(Memory::get().allocate(sizeof(CImage2Outline)));
    if (spacer) {
        const CMqPoint canvasSize{72, 72};
        const Color transparent{};
        CImage2OutlineApi::get().constructor(spacer, &canvasSize, &transparent, 0);
        multilayerApi.addImage(result, spacer, -999, -999);
    }

    const Color edge{32, 38, 45};
    const Color wood{91, 61, 36};
    const Color woodLight{122, 86, 50};
    const Color steelDark{53, 65, 76};
    const Color steel{89, 106, 121};
    const Color steelLight{130, 147, 162};
    const Color steelHighlight{174, 185, 194};

    addFill(result, 1, 1, 6, 0, edge);
    addFill(result, 2, 2, 5, 1, edge);
    addFill(result, 4, 1, 5, 3, edge);
    addFill(result, 7, 1, 5, 4, edge);
    addFill(result, 18, 1, 4, 5, edge);
    addFill(result, 19, 1, 4, 6, edge);
    addFill(result, 16, 1, 4, 7, edge);
    addFill(result, 3, 1, 22, 7, edge);
    addFill(result, 14, 1, 4, 8, edge);
    addFill(result, 12, 1, 4, 9, edge);
    addFill(result, 8, 1, 4, 10, edge);
    addFill(result, 2, 4, 3, 11, edge);
    addFill(result, 1, 1, 3, 15, edge);
    addFill(result, 2, 5, 2, 16, edge);
    addFill(result, 1, 1, 2, 21, edge);
    addFill(result, 2, 2, 1, 22, edge);
    addFill(result, 3, 3, 0, 24, edge);
    addFill(result, 1, 2, 1, 27, edge);
    addFill(result, 1, 3, 0, 29, edge);

    addFill(result, 1, 1, 6, 0, woodLight);
    addFill(result, 1, 6, 5, 1, wood);
    addFill(result, 1, 6, 4, 5, wood);
    addFill(result, 1, 5, 3, 11, wood);
    addFill(result, 1, 6, 2, 16, wood);
    addFill(result, 1, 7, 1, 22, wood);
    addFill(result, 1, 3, 0, 24, wood);
    addFill(result, 1, 3, 0, 29, wood);
    addFill(result, 1, 2, 5, 1, woodLight);
    addFill(result, 1, 2, 3, 11, woodLight);
    addFill(result, 1, 2, 1, 22, woodLight);

    addFill(result, 4, 1, 7, 4, steel);
    addFill(result, 15, 1, 6, 5, steel);
    addFill(result, 16, 1, 6, 6, steel);
    addFill(result, 14, 1, 6, 7, steel);
    addFill(result, 10, 1, 6, 8, steel);
    addFill(result, 9, 1, 6, 9, steelDark);
    addFill(result, 3, 1, 6, 10, edge);
    addFill(result, 5, 1, 14, 6, steelLight);
    addFill(result, 4, 1, 14, 7, steelHighlight);

    return result;
}

} // namespace

game::IMqImage2* getWaitingMovementPathFlag(game::GameImages* images)
{
    if (!images || !images->isoCmon) {
        return nullptr;
    }

    const auto& api = game::GameImagesApi::get();
    if (auto* image = api.getImage(images->isoCmon, "MOVEWAIT", 0, true, nullptr)) {
        return image;
    }
    if (auto* image = createGrayFlag()) {
        return image;
    }
    return api.getImage(images->isoCmon, "MOVENORMAL", 0, true, images->log);
}

} // namespace hooks
