// Exercises the production drawing function with only the game/loader APIs mocked.
// No game, DirectDraw surface, clipboard, installed DLL, or external test framework.
#include "image2memory.h"
#include "mempool.h"
#include "surfacedecompressdata.h"
#include <stb_image_write.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

enum class Layout { Native16, Cnc16, Legacy32 };
Layout layout = Layout::Native16;
int setColorCalls = 0;
int convertColorCalls = 0;
int destroyCalls = 0;

HMODULE WINAPI testGetModuleHandleA(LPCSTR name)
{
    assert(std::strcmp(name, "C4dll-R.dll") == 0);
    return layout == Layout::Native16 ? nullptr : reinterpret_cast<HMODULE>(1);
}

INT_PTR WINAPI dummyExport() { return 0; }

FARPROC WINAPI testGetProcAddress(HMODULE module, LPCSTR name)
{
    assert(module == reinterpret_cast<HMODULE>(1));
    assert(std::strcmp(name, "DDReloadConfig") == 0);
    return layout == Layout::Cnc16 ? dummyExport : nullptr;
}

void __fastcall destroySurface(game::IMqImage2* image, int, char flags)
{
    ++destroyCalls;
    if ((flags & 1) != 0)
        std::free(image);
}

bool __stdcall isDirty(const game::IMqTexture* texture)
{
    const auto* surface = static_cast<const game::CMqImage2Surface16*>(texture);
    return surface->dirty;
}

game::CMqImage2Surface16* __fastcall constructSurface(game::CMqImage2Surface16* surface,
                                                    int, std::uint32_t width,
                                                    std::uint32_t height, int hint,
                                                    int opacity)
{
    assert(hint == 1);
    assert(opacity == 255);
    static game::IMqImage2Vftable imageVftable{};
    static game::IMqTextureVftable textureVftable{};
    imageVftable.destructor = reinterpret_cast<game::IMqImage2Vftable::Destructor>(destroySurface);
    textureVftable.isDirty = isDirty;
    surface->IMqImage2::vftable = &imageVftable;
    surface->IMqTexture::vftable = &textureVftable;
    surface->size = {static_cast<int>(width), static_cast<int>(height)};
    surface->dirty = true;
    return surface;
}

std::uint16_t expectedColor(const game::SurfaceDecompressData& surface,
                            const game::Color& color)
{
    const auto greenMax = surface.rBitMask == 0x7c00 ? 31U : 63U;
    const auto redShift = surface.rBitMask == 0x7c00 ? 10U : 11U;
    return static_cast<std::uint16_t>(((color.r * 31U / 255) << redShift)
                                      | ((color.g * greenMax / 255) << 5)
                                      | (color.b * 31U / 255));
}

std::uint16_t __fastcall convertColor(const game::SurfaceDecompressData* surface,
                                      int, const game::Color* color)
{
    ++convertColorCalls;
    assert(surface->noPalette == 1);
    return expectedColor(*surface, *color);
}

void __fastcall setColor(game::SurfaceDecompressData* surface, int, game::Color color)
{
    ++setColorCalls;
    // Mirrors the native metadata setter: the key is a packed 16-bit value,
    // not the 0x00ff00ff RGB value and not a 32-bit backing-store pixel.
    assert(surface->noPalette == 1);
    assert(color.value <= 0xffff);
    surface->convertedColor = color;
}

void* __cdecl allocate(int size) { return std::malloc(static_cast<std::size_t>(size)); }
void __cdecl release(void* memory) { std::free(memory); }

} // namespace

namespace game::CMqImage2Surface16Api {
Api& get()
{
    static Api api{reinterpret_cast<Api::Constructor>(constructSurface)};
    return api;
}
} // namespace game::CMqImage2Surface16Api

namespace game::SurfaceDecompressDataApi {
Api& get()
{
    static Api api{reinterpret_cast<Api::ConvertColor>(convertColor), nullptr,
                   reinterpret_cast<Api::SetColor>(setColor), nullptr};
    return api;
}
} // namespace game::SurfaceDecompressDataApi

namespace game::Memory {
Api& get()
{
    static Api api{allocate, release};
    return api;
}
} // namespace game::Memory

// The image writer is an unrelated entry point in the included production TU.
extern "C" int stbi_write_png_to_func(stbi_write_func*, void*, int, int, int,
                                       const void*, int)
{
    assert(false && "Image export is outside this test");
    return 0;
}

// Headers have already been included above, so only the two loader calls in
// production code are substituted. Each layout is run in a fresh process to
// exercise its actual cached layout decision independently.
#define GetModuleHandleA testGetModuleHandleA
#define GetProcAddress testGetProcAddress
#include "../mss32/src/image2memory.cpp"
#undef GetProcAddress
#undef GetModuleHandleA

namespace {

void testDraw(std::uint32_t redMask, bool transparent)
{
    auto* image = transparent ? hooks::createImage2Memory(2, 2, true)
                              : hooks::createImage2Memory(2, 2);
    assert(image);
    assert(image->transparent == transparent);
    image->pixels = {game::Color(255, 0, 255, 255), game::Color(0, 0, 0, 255),
                     game::Color(255, 255, 255, 255), game::Color(0, 255, 0, 255)};
    constexpr int guardSize = 16;
    constexpr int nominalPitch = 8;
    const int physicalPitch = layout == Layout::Legacy32 ? nominalPitch * 2 : nominalPitch;
    std::vector<std::uint8_t> actual(guardSize * 2 + physicalPitch * 2, 0xa5);
    auto expected = actual;
    game::SurfaceDecompressData surface{};
    surface.noPalette = 1;
    surface.surfaceMemory = actual.data() + guardSize;
    surface.textureArea = {0, 0, 2, 2};
    surface.pitch = nominalPitch;
    surface.convertedColor = game::Color(0xffffffff);
    surface.rBitMask = redMask;
    surface.gBitMask = redMask == 0x7c00 ? 0x03e0 : 0x07e0;
    surface.bBitMask = 0x001f;

    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const auto& color = image->pixels[y * 2 + x];
            const auto offset = guardSize + y * physicalPitch;
            if (layout == Layout::Legacy32) {
                std::memcpy(expected.data() + offset + x * sizeof(game::Color),
                            &color, sizeof(color));
            } else {
                const auto packed = expectedColor(surface, color);
                std::memcpy(expected.data() + offset + x * sizeof(packed),
                            &packed, sizeof(packed));
            }
        }
    }

    setColorCalls = 0;
    convertColorCalls = 0;
    auto* texture = static_cast<game::IMqTexture*>(image);
    texture->vftable->draw(texture, &surface);
    const auto key = redMask == 0x7c00 ? 0x7c1fU : 0xf81fU;
    assert(surface.convertedColor.value == (transparent ? key : 0xffffffff));
    assert(setColorCalls == (transparent ? 1 : 0));
    assert(convertColorCalls == (transparent ? 1 : 0)
                                   + (layout == Layout::Legacy32 ? 0 : 4));
    assert(actual == expected); // Includes row padding and guard regions.
    assert(!image->dirty);
    const auto beforeDestroy = destroyCalls;
    image->IMqImage2::vftable->destructor(image, 1);
    assert(destroyCalls == beforeDestroy + 1);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    if (std::strcmp(argv[1], "native") == 0)
        layout = Layout::Native16;
    else if (std::strcmp(argv[1], "cnc") == 0)
        layout = Layout::Cnc16;
    else if (std::strcmp(argv[1], "legacy") == 0)
        layout = Layout::Legacy32;
    else
        return 2;
    testDraw(0xf800, true);
    testDraw(0xf800, false);
    testDraw(0x7c00, true);
    testDraw(0x7c00, false);
    std::printf("image2memory transparency: %s passed (565/555, keyed/opaque, pitch/guards)\n",
                argv[1]);
    return 0;
}
