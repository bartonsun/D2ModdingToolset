/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2026
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
 */

#ifndef BAGDROPTOMBHOOKS_H
#define BAGDROPTOMBHOOKS_H

#include "idlist.h"

namespace game {
struct CMidgardPlan;
struct CMqPoint;
}

namespace hooks {

bool __fastcall getObjectsAtPointHooked(const game::CMidgardPlan* thisptr,
                                        int /*%edx*/,
                                        game::IdList* objectIds,
                                        const game::CMqPoint* mapPosition);

} // namespace hooks

#endif // BAGDROPTOMBHOOKS_H
