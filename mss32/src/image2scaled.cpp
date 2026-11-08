#include "image2scaled.h"
#include "d2color.h"
#include "image2memory.h"
#include "mqimage2.h"
#include "mqpresentationmanager.h"
#include "mqrenderer2.h"
#include "mqtexture.h"
#include "rendererimpl.h"
#include "surfacedecompressdata.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace hooks {
namespace {

constexpr int maxCaptureSide = 1024;
constexpr std::uint32_t transparentRgb = 0x00ff00ff;

bool validSize(const game::CMqPoint& size)
{
    return size.x > 0 && size.y > 0 && size.x <= maxCaptureSide
           && size.y <= maxCaptureSide;
}

struct CaptureRenderer : game::IMqRenderer2
{
    game::CRendererImpl* nativeRenderer{};
    game::CMqPoint size{};
    std::vector<game::Color> pixels;
    bool failed{};
    unsigned int layers{};
};

const game::RenderData22* findTexture(const CaptureRenderer& capture,
                                    const game::TextureHandle& handle)
{
    const auto pair = capture.nativeRenderer->data40IntPair;
    if (!pair || !*pair || !handle.indexPtr)
        return nullptr;
    // This is the documented native handle registry, not an image-class cast.
    for (const auto& entry : (*pair)->first) {
        if (entry.textureHandle.indexPtr
            && *entry.textureHandle.indexPtr == *handle.indexPtr) {
            return &entry.data22;
        }
    }
    return nullptr;
}

std::uint8_t expandChannel(std::uint16_t value, std::uint32_t mask)
{
    if (!mask)
        return 0;
    unsigned int shift = 0;
    while ((mask & 1) == 0) {
        ++shift;
        mask >>= 1;
    }
    return static_cast<std::uint8_t>((((value >> shift) & mask) * 255 + mask / 2)
                                     / mask);
}

bool decodeTexture(CaptureRenderer& capture,
                   const game::RenderData22& textureData,
                   std::vector<game::Color>& pixels)
{
    using namespace game;
    if (!textureData.texture || !validSize(textureData.textureSize)
        || textureData.opacity < 0 || textureData.opacity > 255) {
        return false;
    }
    const auto size = textureData.textureSize;
    const auto count = static_cast<std::size_t>(size.x) * size.y;
    SurfaceDecompressData surface{};
    surface.textureArea = {0, 0, size.x, size.y};
    // The native decoder writes a palette index or a packed color key here.
    // The all-ones sentinel means that no transparent color was registered.
    surface.convertedColor = Color(0xffffffff);
    auto* texturer = static_cast<IMqTexturer2*>(capture.nativeRenderer);
    if (textureData.hasCustomPalette) {
        if (!texturer->vftable->getPaletteEntries(texturer, textureData.paletteKey,
                                                 surface.paletteEntries)) {
            return false;
        }
        // Native FF textures support an indexed surface directly; this avoids
        // the wrapper-dependent 16/32-bit color-conversion routines entirely.
        surface.noPalette = 0;
        surface.pitch = size.x;
        std::vector<std::uint8_t> indexed(count, 0);
        surface.surfaceMemory = indexed.data();
        textureData.texture->vftable->draw(textureData.texture, &surface);
        pixels.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (surface.convertedColor.value == indexed[i]) {
                pixels[i] = Color(255, 0, 255, 255);
                continue;
            }
            const auto& entry = surface.paletteEntries[indexed[i]];
            pixels[i] = Color(entry.peRed, entry.peGreen, entry.peBlue, 255);
        }
        return true;
    }

    // Unpaletted native surfaces use RGB565. Legacy C4dll-R substitutes a
    // 32-bit backing store while retaining a nominal 16-bit pitch; cnc-ddraw
    // (identified by DDReloadConfig) and the original renderer use true 16-bit.
    const HMODULE wrapper = GetModuleHandleA("C4dll-R.dll");
    const bool legacy32 = wrapper && !GetProcAddress(wrapper, "DDReloadConfig");
    surface.noPalette = 1;
    surface.pitch = size.x * 2;
    surface.rBitMask = 0xf800;
    surface.gBitMask = 0x07e0;
    surface.bBitMask = 0x001f;
    pixels.assign(count, Color(255, 0, 255, 255));
    if (legacy32) {
        surface.surfaceMemory = pixels.data();
        textureData.texture->vftable->draw(textureData.texture, &surface);
    } else {
        std::vector<std::uint16_t> packed(count, 0xf81f);
        surface.surfaceMemory = packed.data();
        textureData.texture->vftable->draw(textureData.texture, &surface);
        for (std::size_t i = 0; i < count; ++i) {
            pixels[i] = Color(expandChannel(packed[i], surface.rBitMask),
                              expandChannel(packed[i], surface.gBitMask),
                              expandChannel(packed[i], surface.bBitMask), 255);
        }
    }
    return true;
}

void __fastcall captureTexture(game::IMqRenderer2* renderer, int,
                               game::TextureHandle* handle,
                               const game::CMqPoint* start,
                               const game::CMqPoint* offset,
                               const game::CMqPoint* size,
                               const game::CMqRect* area)
{
    auto& capture = *static_cast<CaptureRenderer*>(renderer);
    if (capture.failed)
        return;
    if (!handle || !start || !offset || !size || !validSize(*size)) {
        capture.failed = true;
        return;
    }
    const auto* data = findTexture(capture, *handle);
    std::vector<game::Color> pixels;
    if (!data || !decodeTexture(capture, *data, pixels)) {
        capture.failed = true;
        return;
    }
    ++capture.layers;
    for (int y = 0; y < size->y; ++y) {
        const int dy = start->y + y;
        const int sy = offset->y + y;
        if (dy < 0 || dy >= capture.size.y || sy < 0 || sy >= data->textureSize.y
            || (area && (dy < area->top || dy >= area->bottom)))
            continue;
        for (int x = 0; x < size->x; ++x) {
            const int dx = start->x + x;
            const int sx = offset->x + x;
            if (dx < 0 || dx >= capture.size.x || sx < 0 || sx >= data->textureSize.x
                || (area && (dx < area->left || dx >= area->right)))
                continue;
            const auto color = pixels[sy * data->textureSize.x + sx];
            if ((color.value & 0x00ffffff) == transparentRgb)
                continue;
            auto& destination = capture.pixels[dy * capture.size.x + dx];
            const unsigned int alpha = static_cast<unsigned int>(data->opacity);
            if (alpha == 255) {
                destination = color;
            } else {
                destination = game::Color(
                    static_cast<std::uint8_t>((color.r * alpha + destination.r * (255 - alpha)) / 255),
                    static_cast<std::uint8_t>((color.g * alpha + destination.g * (255 - alpha)) / 255),
                    static_cast<std::uint8_t>((color.b * alpha + destination.b * (255 - alpha)) / 255),
                    255);
            }
        }
    }
}

int __fastcall captureBatch(game::IMqRenderer2*, int) { return 0; }
float __fastcall captureFps(const game::IMqRenderer2*, int) { return 0; }
void __fastcall rejectOperation(game::IMqRenderer2* renderer, int)
{
    static_cast<CaptureRenderer*>(renderer)->failed = true;
}
void __fastcall rejectArea(game::IMqRenderer2* renderer, int, const game::CMqRect*)
{
    static_cast<CaptureRenderer*>(renderer)->failed = true;
}
void __fastcall captureStats(const game::IMqRenderer2*, int, game::RenderStatistics* stats)
{
    if (stats)
        *stats = {};
}
int __fastcall rejectFrame(game::IMqRenderer2* renderer, int)
{
    static_cast<CaptureRenderer*>(renderer)->failed = true;
    return 0;
}
void __fastcall rejectMethod9(game::IMqRenderer2* renderer, int, int, int, int)
{
    static_cast<CaptureRenderer*>(renderer)->failed = true;
}

game::IMqRenderer2Vftable captureVftable{
    reinterpret_cast<game::IMqRenderer2Vftable::GetBatchNumber>(captureBatch),
    reinterpret_cast<game::IMqRenderer2Vftable::GetFps>(captureFps),
    reinterpret_cast<game::IMqRenderer2Vftable::BeforeRender>(rejectOperation),
    reinterpret_cast<game::IMqRenderer2Vftable::RenderFrame>(rejectFrame),
    reinterpret_cast<game::IMqRenderer2Vftable::DrawTexture>(captureTexture),
    reinterpret_cast<game::IMqRenderer2Vftable::AddArea>(rejectArea),
    reinterpret_cast<game::IMqRenderer2Vftable::RemoveArea>(rejectOperation),
    reinterpret_cast<game::IMqRenderer2Vftable::GetRenderStats>(captureStats),
    reinterpret_cast<game::IMqRenderer2Vftable::ResetRenderStats>(rejectOperation),
    reinterpret_cast<game::IMqRenderer2Vftable::Method9>(rejectMethod9),
    nullptr,
    nullptr,
    reinterpret_cast<game::IMqRenderer2Vftable::Method12>(rejectOperation),
};

} // namespace

game::IMqImage2* createScaledImage(game::IMqImage2* source,
                                 const game::CMqPoint& targetSize)
{
    using namespace game;
    if (!source || !validSize(targetSize))
        return nullptr;
    CMqPoint sourceSize{};
    source->vftable->getSize(source, &sourceSize);
    if (!validSize(sourceSize))
        return nullptr;
    PresentationMgrPtr manager{};
    const auto& presentation = CMqPresentationManagerApi::get();
    presentation.getPresentationManager(&manager);
    auto* renderer = manager.data && manager.data->data ? manager.data->data->renderer : nullptr;
    if (!renderer) {
        presentation.presentationMgrPtrSetData(&manager, nullptr);
        return nullptr;
    }
    CaptureRenderer capture{};
    capture.vftable = &captureVftable;
    capture.nativeRenderer = renderer;
    capture.size = sourceSize;
    capture.pixels.assign(static_cast<std::size_t>(sourceSize.x) * sourceSize.y,
                          Color(255, 0, 255, 255));
    const CMqPoint origin{};
    const CMqRect area{0, 0, sourceSize.x, sourceSize.y};
    source->vftable->render(source, &capture, &origin, &origin, &sourceSize, &area);
    presentation.presentationMgrPtrSetData(&manager, nullptr);
    if (capture.failed || capture.layers == 0)
        return nullptr;
    auto* result = createImage2Memory(targetSize.x, targetSize.y, true);
    if (!result)
        return nullptr;
    // Point sampling preserves the native transparent color key at the frame's
    // corners and never introduces magenta interpolation fringes.
    for (int y = 0; y < targetSize.y; ++y) {
        const int sy = (2 * y + 1) * sourceSize.y / (2 * targetSize.y);
        for (int x = 0; x < targetSize.x; ++x) {
            const int sx = (2 * x + 1) * sourceSize.x / (2 * targetSize.x);
            result->pixels[y * targetSize.x + x] = capture.pixels[sy * sourceSize.x + sx];
        }
    }
    result->dirty = true;
    return result;
}

} // namespace hooks
