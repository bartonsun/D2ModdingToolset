/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "waitingmovepath.h"
#include "version.h"

#include <array>

namespace game::WaitingMovementPathApi {

static const std::array<Api, 4> functions = {{
    Api{
        (ITaskVftable*)0x6dca1c,
        (ITaskVftable*)0x6dc9dc,
        (ITaskVftable*)0x6dcbec,
        (ITaskVftable*)0x6efadc,
        (ITaskVftable*)0x6dce54,
        (Api::PathConstructor)0x4cccc0,
        (Api::PathDestructor)0x4cceee,
        (Api::PathUpdate)0x4cd406,
    },
    Api{
        (ITaskVftable*)0x6dca1c,
        (ITaskVftable*)0x6dc9dc,
        (ITaskVftable*)0x6dcbec,
        (ITaskVftable*)0x6efadc,
        (ITaskVftable*)0x6dce54,
        (Api::PathConstructor)0x4cccc0,
        (Api::PathDestructor)0x4cceee,
        (Api::PathUpdate)0x4cd406,
    },
    Api{
        (ITaskVftable*)0x6da9bc,
        (ITaskVftable*)0x6da97c,
        (ITaskVftable*)0x6dab8c,
        (ITaskVftable*)0x6eda7c,
        (ITaskVftable*)0x6dadf4,
        (Api::PathConstructor)0x4cc3b3,
        (Api::PathDestructor)0x4cc5e1,
        (Api::PathUpdate)0x4ccaff,
    },
    Api{},
}};

const Api* get()
{
    const auto version = hooks::gameVersion();
    if (version == hooks::GameVersion::Unknown) {
        return nullptr;
    }

    const auto& api = functions[static_cast<int>(version)];
    return api.taskSelectUnitVftable ? &api : nullptr;
}

}
