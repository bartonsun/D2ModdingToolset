#include "waitingmovepathflag.h"
#include "image2fill.h"
#include "image2outline.h"
#include "mempool.h"
#include "multilayerimg.h"

#include <cstdint>

namespace hooks {
namespace {

game::CImage2Fill* createFill(std::uint32_t width,
                              std::uint32_t height,
                              const game::Color& color)
{
    using namespace game;

    auto* image = static_cast<CImage2Fill*>(Memory::get().allocate(sizeof(CImage2Fill)));
    if (!image) {
        return nullptr;
    }

    const auto& fillApi = CImage2FillApi::get();
    fillApi.constructor(image, width, height);
    fillApi.setColor(image, &color);
    return image;
}

void addFill(game::CMultiLayerImg* image,
             std::uint32_t width,
             std::uint32_t height,
             int x,
             int y,
             const game::Color& color)
{
    if (auto* fill = createFill(width, height, color)) {
        game::CMultiLayerImgApi::get().addImage(image, fill, x, y);
    }
}

void addFlagFill(game::CMultiLayerImg* image,
                 std::uint32_t width,
                 std::uint32_t height,
                 int x,
                 int y,
                 const game::Color& color)
{
    addFill(image, width, height, x + 35, y + 8, color);
}

} // namespace

game::IMqImage2* createWaitingMovementPathGrayFlag()
{
    using namespace game;

    auto* result = static_cast<CMultiLayerImg*>(Memory::get().allocate(sizeof(CMultiLayerImg)));
    if (!result) {
        return nullptr;
    }

    const auto& multilayerApi = CMultiLayerImgApi::get();
    multilayerApi.constructor(result);

    auto* spacer = static_cast<CImage2Outline*>(
        Memory::get().allocate(sizeof(CImage2Outline)));
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

    addFlagFill(result, 1, 1, 6, 0, edge);
    addFlagFill(result, 2, 2, 5, 1, edge);
    addFlagFill(result, 4, 1, 5, 3, edge);
    addFlagFill(result, 7, 1, 5, 4, edge);
    addFlagFill(result, 18, 1, 4, 5, edge);
    addFlagFill(result, 19, 1, 4, 6, edge);
    addFlagFill(result, 16, 1, 4, 7, edge);
    addFlagFill(result, 3, 1, 22, 7, edge);
    addFlagFill(result, 14, 1, 4, 8, edge);
    addFlagFill(result, 12, 1, 4, 9, edge);
    addFlagFill(result, 8, 1, 4, 10, edge);
    addFlagFill(result, 2, 4, 3, 11, edge);
    addFlagFill(result, 1, 1, 3, 15, edge);
    addFlagFill(result, 2, 5, 2, 16, edge);
    addFlagFill(result, 1, 1, 2, 21, edge);
    addFlagFill(result, 2, 2, 1, 22, edge);
    addFlagFill(result, 3, 3, 0, 24, edge);
    addFlagFill(result, 1, 2, 1, 27, edge);
    addFlagFill(result, 1, 3, 0, 29, edge);

    addFlagFill(result, 1, 1, 6, 0, woodLight);
    addFlagFill(result, 1, 6, 5, 1, wood);
    addFlagFill(result, 1, 6, 4, 5, wood);
    addFlagFill(result, 1, 5, 3, 11, wood);
    addFlagFill(result, 1, 6, 2, 16, wood);
    addFlagFill(result, 1, 7, 1, 22, wood);
    addFlagFill(result, 1, 3, 0, 24, wood);
    addFlagFill(result, 1, 3, 0, 29, wood);
    addFlagFill(result, 1, 2, 5, 1, woodLight);
    addFlagFill(result, 1, 2, 3, 11, woodLight);
    addFlagFill(result, 1, 2, 1, 22, woodLight);

    addFlagFill(result, 4, 1, 7, 4, steel);
    addFlagFill(result, 15, 1, 6, 5, steel);
    addFlagFill(result, 16, 1, 6, 6, steel);
    addFlagFill(result, 14, 1, 6, 7, steel);
    addFlagFill(result, 10, 1, 6, 8, steel);
    addFlagFill(result, 9, 1, 6, 9, steelDark);
    addFlagFill(result, 3, 1, 6, 10, edge);
    addFlagFill(result, 5, 1, 14, 6, steelLight);
    addFlagFill(result, 4, 1, 14, 7, steelHighlight);

    return result;
}

} // namespace hooks
