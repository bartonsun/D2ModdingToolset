/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/Rapthos/Experimental-version)
 * Copyright (C) 2026 Rapthos.
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

#ifndef BATATTACKDEFENDHOOKS_H
#define BATATTACKDEFENDHOOKS_H

#include "batattack.h"
#include "midgardid.h"

namespace game {

struct IAttack;

struct CBatAttackDefend : public CBatAttackBase
{
    CMidgardID unitId;
};

assert_size(CBatAttackDefend, 8);

namespace CBatAttackDefendApi {

IBatAttackVftable* vftable();

} // namespace CBatAttackDefendApi

} // namespace game

#endif // BATATTACKDEFENDHOOKS_H
