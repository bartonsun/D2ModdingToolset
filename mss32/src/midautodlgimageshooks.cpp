/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Stanislav Egorov.
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

#include "midautodlgimageshooks.h"
#include "..\resource.h"
#include "gameimagedata.h"
#include "gameimages.h"
#include "logfile.h"
#include "mempool.h"
#include "midautodlgimages.h"
#include "mqdb.h"
#include "utils.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <string>

extern HMODULE library;

namespace hooks {

class CDefaultCustomLobbyImages
{
public:
    CDefaultCustomLobbyImages();
    ~CDefaultCustomLobbyImages();
    game::GameImageDataWrapper* get();

private:
    bool write(HANDLE file);

    bool initialized;
    game::GameImageDataWrapper* ptr;
};

CDefaultCustomLobbyImages::CDefaultCustomLobbyImages()
    : initialized(false)
    , ptr(nullptr)
{ }

CDefaultCustomLobbyImages::~CDefaultCustomLobbyImages()
{
    using namespace game;

    if (ptr) {
        GameImageDataApi::get().destructor(ptr);
        Memory::get().freeNonZero(ptr);
    }
}

game::GameImageDataWrapper* CDefaultCustomLobbyImages::get()
{
    if (!initialized) {
        initialized = true;

        const auto path{interfFolder() / "CustomLobby.ff"};
        auto file = ::CreateFile(path.string().c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            if (::GetLastError() != ERROR_ALREADY_EXISTS) {
                write(file);
            }
            ::CloseHandle(file);
        }
        game::GameImageDataApi::get().loadFromFile(&ptr, path.string().c_str(), 1);
    }

    return ptr;
}

bool CDefaultCustomLobbyImages::write(HANDLE file)
{
    HRSRC resourceInfo = ::FindResource(library, MAKEINTRESOURCE(FF_CUSTOM_LOBBY), RT_RCDATA);
    if (resourceInfo == NULL) {
        return false;
    }

    DWORD resourceSize = ::SizeofResource(library, resourceInfo);
    if (resourceSize == 0) {
        return false;
    }

    HGLOBAL resource = ::LoadResource(library, resourceInfo);
    if (resource == NULL) {
        return false;
    }

    DWORD written;
    auto data = reinterpret_cast<const char*>(::LockResource(resource));
    if (!::WriteFile(file, data, resourceSize, &written, NULL) || written != resourceSize) {
        return false;
    }

    return true;
}

game::IMqImage2* __fastcall midAutoDlgImagesLoadImageHooked(game::CMidAutoDlgImages* thisptr,
                                                            int /*%edx*/,
                                                            const char* imageName,
                                                            const game::CMqPoint* /*imageSize*/)
{
    using namespace game;

    static CDefaultCustomLobbyImages defaultLobbyImages;

    const auto& gameImagesApi = GameImagesApi::get();

    if (!imageName || !strlen(imageName)) {
        return nullptr;
    }

    auto images = thisptr->imagesData;
    auto result = gameImagesApi.getImage(images, imageName, 0, false, thisptr->logFile);
    if (!result) {
        auto lobbyImages = defaultLobbyImages.get();
        if (lobbyImages) {
            result = gameImagesApi.getImage(lobbyImages, imageName, 0, false, thisptr->logFile);
        }
        if (!result) {
            auto log = thisptr->logFile;
            if (log) {
                auto mqdb = *images->data->imagesData->data->mqdb;

                char message[1024] = {};
                sprintf_s(message, "InterfUtils: Error loading image %s from %s", imageName,
                          mqdb->filePath.string);
                log->vftable->log(log, message);
            }
        }
    }

    return result;
}

} // namespace hooks
