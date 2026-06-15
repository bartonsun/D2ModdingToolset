/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2023 Vladimir Makeev.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "image2memory.h"
#include "mempool.h"
#include "surfacedecompressdata.h"
#include <stb_image_write.h>

namespace hooks {

// CMqImage2Surface16::IMqImage2 original destructor, it will properly destroy CImage2Memory objects
static game::IMqImage2Vftable::Destructor image2SurfaceDtor = nullptr;
// Custom vftable for CImage2Memory::IMqImage2
static game::IMqImage2Vftable image2MemoryImage2Vftable{};
// Custom vftable for CImage2Memory::IMqTexture
static game::IMqTextureVftable image2MemoryTextureVftable{};

static void __fastcall img2MemoryDtor(CImage2Memory* thisptr, int /* %edx */, char flags)
{
    thisptr->pixels.~vector();

    if (image2SurfaceDtor) {
        image2SurfaceDtor(thisptr, flags);
    }
}

static CImage2Memory* textureToImage(game::IMqTexture* thisptr)
{
    using namespace game;

    const std::intptr_t offset = offsetof(CImage2Memory, IMqTexture::vftable);

    const auto rawPtr = reinterpret_cast<std::uintptr_t>(thisptr) - offset;
    CImage2Memory* img2Mem = reinterpret_cast<CImage2Memory*>(rawPtr);

    return img2Mem;
}

static void __stdcall img2MemoryDraw(game::IMqTexture* thisptr,
                                     game::SurfaceDecompressData* decompressData)
{
    using namespace game;

    CImage2Memory* img2Mem = textureToImage(thisptr);

    // Do actual drawing onto decompressData memory
    if (decompressData->noPalette) {
        const int height = img2Mem->size.y;
        const int width = img2Mem->size.x;

        const int pitch = decompressData->pitch * 2;
        char* dst = reinterpret_cast<char*>(decompressData->surfaceMemory);

        for (int i = 0; i < height; ++i, dst += pitch) {
            std::memcpy(dst, &img2Mem->pixels[i * width], width * sizeof(Color));
        }
    }

    img2Mem->dirty = false;
}

CImage2Memory::CImage2Memory(std::uint32_t width, std::uint32_t height)
    : pixels(width * height, game::Color(255, 0, 255, 255))
{
    game::CMqImage2Surface16Api::get().constructor(this, width, height, 1, 0xff);
}

CImage2Memory* createImage2Memory(std::uint32_t width, std::uint32_t height)
{
    using namespace game;

    CImage2Memory* img2Memory = (CImage2Memory*)Memory::get().allocate(sizeof(CImage2Memory));
    new (img2Memory) CImage2Memory(width, height);

    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;

        // Reuse CMqImage2Surface16::IMqImage2 vftable
        std::memcpy(&image2MemoryImage2Vftable, img2Memory->IMqImage2::vftable,
                    sizeof(IMqImage2Vftable));

        // Use Destructor and IsDirty methods from CMqImage2Surface16::IMqTexture vftable
        std::memcpy(&image2MemoryTextureVftable, img2Memory->IMqTexture::vftable,
                    sizeof(IMqTextureVftable));

        // Use custom draw method
        image2MemoryTextureVftable.draw = img2MemoryDraw;
        // Save original destructor for proper base class destrution
        image2SurfaceDtor = image2MemoryImage2Vftable.destructor;
        // Use custom destructor
        image2MemoryImage2Vftable.destructor = (IMqImage2Vftable::Destructor)img2MemoryDtor;
    }

    // Switch to our vftables
    img2Memory->IMqImage2::vftable = &image2MemoryImage2Vftable;
    img2Memory->IMqTexture::vftable = &image2MemoryTextureVftable;

    return img2Memory;
}

void hooks::writeImageToMemory(const CImage2Memory* image, std::vector<uint8_t>& out)
{
    const int width = image->size.x;
    const int height = image->size.y;

    stbi_write_png_to_func(
        [](void* context, void* data, int size) {
            auto* vec = static_cast<std::vector<uint8_t>*>(context);
            vec->insert(vec->end(), (uint8_t*)data, (uint8_t*)data + size);
        },
        &out, width, height, 4,
        image->pixels.data(), width * 4);
}

void copyImageToClipboard(const CImage2Memory* image)
{
    if (!image || image->pixels.empty())
        return;

    // 1. PNG
    std::vector<uint8_t> pngData;
    writeImageToMemory(image, pngData);

    // 2. DIB
    const int width = image->size.x;
    const int height = image->size.y;
    const int bpp = 24;
    const int rowSize = ((width * bpp + 31) / 32) * 4;
    const int dataSize = rowSize * height;
    const int headerSize = sizeof(BITMAPINFOHEADER);

    HGLOBAL hDib = GlobalAlloc(GMEM_MOVEABLE, headerSize + dataSize);
    if (hDib) {
        BYTE* pData = (BYTE*)GlobalLock(hDib);
        BITMAPINFOHEADER* pHeader = (BITMAPINFOHEADER*)pData;
        pHeader->biSize = sizeof(BITMAPINFOHEADER);
        pHeader->biWidth = width;
        pHeader->biHeight = -height;
        pHeader->biPlanes = 1;
        pHeader->biBitCount = bpp;
        pHeader->biCompression = BI_RGB;
        pHeader->biSizeImage = dataSize;
        pHeader->biXPelsPerMeter = 0;
        pHeader->biYPelsPerMeter = 0;
        pHeader->biClrUsed = 0;
        pHeader->biClrImportant = 0;
        BYTE* pPixels = pData + headerSize;
        for (int y = 0; y < height; ++y) {
            BYTE* row = pPixels + y * rowSize;
            for (int x = 0; x < width; ++x) {
                const game::Color& color = image->pixels[y * width + x];
                row[x * 3 + 0] = color.b;
                row[x * 3 + 1] = color.g;
                row[x * 3 + 2] = color.r;
            }
        }
        GlobalUnlock(hDib);
    }

    // To clipboard
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();

        // PNG format
        if (!pngData.empty()) {
            HGLOBAL hPng = GlobalAlloc(GMEM_MOVEABLE, pngData.size());
            if (hPng) {
                memcpy(GlobalLock(hPng), pngData.data(), pngData.size());
                GlobalUnlock(hPng);
                UINT pngFormat = RegisterClipboardFormatA("PNG");
                SetClipboardData(pngFormat, hPng);
            }
        }

        // DIB format
        if (hDib) {
            SetClipboardData(CF_DIB, hDib);
        }

        CloseClipboard();
    } else {
        if (hDib)
            GlobalFree(hDib);
    }
}

} // namespace hooks
