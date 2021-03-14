/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Vladimir Makeev.
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

#ifndef BATATTACKDOPPELGANGER_H
#define BATATTACKDOPPELGANGER_H

#include "batattack.h"
#include "midgardid.h"

namespace game {

struct IAttack;

/** Represents doppelganger attacks. */
struct CBatAttackDoppelganger : public CBatAttackBase
{
    CMidgardID unitId;
    CMidgardID id2;
    int attackNumber;
    IAttack* attack;
    bool unknown;
    bool unknown2;
    char padding[2];
    IBatAttack* altAttack;
};

static_assert(sizeof(CBatAttackDoppelganger) == 28,
              "Size of CBatAttackDoppelganger structure must be exactly 28 bytes");

namespace CBatAttackDoppelgangerApi {

struct Api
{
    IBatAttackVftable::OnAttack onHit;
};

Api& get();

} // namespace CBatAttackDoppelgangerApi

} // namespace game

#endif // BATATTACKDOPPELGANGER_H