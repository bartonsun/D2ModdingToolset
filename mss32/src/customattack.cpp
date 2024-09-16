/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2020 Vladimir Makeev.
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

#include "customattack.h"
#include "attackclasscat.h"
#include "battlemsgdata.h"
#include "customattackhooks.h"
#include "game.h"
#include "mempool.h"
#include <spdlog/spdlog.h>

namespace hooks {

void __fastcall customAttackDtor(CustomAttack* attack, int /*%edx*/, bool freeMemory)
{
    spdlog::debug("CustomAttack d-tor");
    if (freeMemory) {
        game::Memory::get().freeNonZero(attack);
    }
}

bool __fastcall customAttackCanPerform(CustomAttack* thisptr,
                                       int /*%edx*/,
                                       game::IMidgardObjectMap* objectMap,
                                       game::BattleMsgData* battleMsgData,
                                       game::CMidgardID* unitId)
{
    spdlog::debug("CustomAttack canPerform");
    return true;
}

game::CMidgardID* __fastcall customAttackGetTargetGroupId(CustomAttack* thisptr,
                                                          int /*%edx*/,
                                                          game::CMidgardID* targetGroupId,
                                                          game::BattleMsgData* battleMsgData)
{
    spdlog::debug("CustomAttack getTargetGroupId");
    game::gameFunctions().getAllyOrEnemyGroupId(targetGroupId, battleMsgData, &thisptr->id1, false);
    return targetGroupId;
}

void __fastcall customAttackFillTargetsList(CustomAttack* thisptr,
                                            int /*%edx*/,
                                            game::IMidgardObjectMap* objectMap,
                                            game::BattleMsgData* battleMsgData,
                                            game::TargetSet* targetsList)
{
    spdlog::debug("CustomAttack fillTargetsList");
    game::BattleMsgDataApi::get().fillTargetsList(objectMap, battleMsgData, thisptr, &thisptr->id1,
                                                  &thisptr->id2, false, targetsList, true);
}

void __fastcall customAttackFillAltTargetsList(CustomAttack* thisptr,
                                               int /*%edx*/,
                                               game::IMidgardObjectMap* objectMap,
                                               game::BattleMsgData* battleMsgData,
                                               game::TargetSet* targetsList)
{
    spdlog::debug("CustomAttack fillAltTargetsList");
    // do nothing
}

bool __fastcall customAttackMethod5(CustomAttack* thisptr,
                                    int /*%edx*/,
                                    game::IMidgardObjectMap* objectMap,
                                    game::BattleMsgData* battleMsgData,
                                    game::CMidgardID* unitId)
{
    spdlog::debug("CustomAttack method 5");
    return thisptr->vftable->canPerform(thisptr, objectMap, battleMsgData, unitId);
}

bool __fastcall customAttackCanMiss(CustomAttack* thisptr,
                                    int /*%edx*/,
                                    game::BattleMsgData* battleMsgData,
                                    game::CMidgardID* id)
{
    spdlog::debug("CustomAttack canMiss");
    return true;
}

bool __fastcall customAttackMethod7(CustomAttack* thisptr, int /*%edx*/, int a2, int a3, int a4)
{
    spdlog::debug("CustomAttack method 7");
    return false;
}

bool __fastcall customAttackIsImmune(CustomAttack* thisptr,
                                     int /*%edx*/,
                                     game::IMidgardObjectMap* objectMap,
                                     game::BattleMsgData* battleMsgData,
                                     game::CMidgardID* unitId)
{
    spdlog::debug("CustomAttack isImmune");
    return game::gameFunctions().isUnitImmuneToAttack(objectMap, battleMsgData, unitId,
                                                      thisptr->attack, false);
}

void __fastcall customAttackOnMiss(CustomAttack* thisptr,
                                   int /*%edx*/,
                                   int a2,
                                   int a3,
                                   int a4,
                                   int a5)
{
    spdlog::debug("CustomAttack onMiss");
    // do nothing
}

bool __fastcall customAttackGetAttackClass(CustomAttack* thisptr,
                                           int /*%edx*/,
                                           const game::CMidgardID* targetUnitId,
                                           const game::BattleMsgData* battleMsgData,
                                           game::LAttackClass* attackClass)
{
    spdlog::debug("CustomAttack getAttackClass");
    attackClass->id = customAttackClass.id;
    attackClass->table = customAttackClass.table;
    return true;
}

bool __fastcall customAttackGetUnderlyingAttackClass(CustomAttack* thisptr,
                                                     int /*%edx*/,
                                                     const game::CMidgardID* targetUnitId,
                                                     const game::BattleMsgData* battleMsgData,
                                                     game::LAttackClass* attackClass)
{
    spdlog::debug("CustomAttack getUnderlyingAttackClass");
    return thisptr->vftable->getAttackClass(thisptr, targetUnitId, battleMsgData, attackClass);
}

void __fastcall customAttackDoAttack(CustomAttack* thisptr,
                                     int /*%edx*/,
                                     game::IMidgardObjectMap* objectMap,
                                     game::BattleMsgData* battleMsgData,
                                     game::CMidgardID* unitId,
                                     void* a5)
{
    spdlog::debug("CustomAttack doAttack");
}

bool __fastcall customAttackMethod13(CustomAttack* thisptr,
                                     int /*%edx*/,
                                     game::BattleMsgData* battleMsgData,
                                     game::IMidgardObjectMap* objectMap)
{
    spdlog::debug("CustomAttack method 13");
    return false;
}

bool __fastcall customAttackMethod14(CustomAttack* thisptr,
                                     int /*%edx*/,
                                     game::BattleMsgData* battleMsgData)
{
    spdlog::debug("CustomAttack method 14");
    return false;
}

bool __fastcall customAttackMethod15(CustomAttack* thisptr,
                                     int /*%edx*/,
                                     game::BattleMsgData* battleMsgData)
{
    spdlog::debug("CustomAttack method 15");
    return false;
}

bool __fastcall customAttackMethod16(CustomAttack* thisptr,
                                     int /*%edx*/,
                                     game::BattleMsgData* battleMsgData)
{
    spdlog::debug("CustomAttack method 16");
    return false;
}

bool __fastcall customAttackMethod17(CustomAttack* thisptr,
                                     int /*%edx*/,
                                     game::BattleMsgData* battleMsgData)
{
    spdlog::debug("CustomAttack method 17");
    return false;
}

// clang-format off
static const game::IBatAttackVftable customAttackVftable{
    (game::IBatAttackVftable::Destructor)customAttackDtor,
    (game::IBatAttackVftable::CanPerform)customAttackCanPerform,
    (game::IBatAttackVftable::GetTargetGroupId)customAttackGetTargetGroupId,
    (game::IBatAttackVftable::FillTargetsList)customAttackFillTargetsList,
    (game::IBatAttackVftable::FillTargetsList)customAttackFillAltTargetsList,
    (game::IBatAttackVftable::Method5)customAttackMethod5,
    (game::IBatAttackVftable::CanMiss)customAttackCanMiss,
    (game::IBatAttackVftable::Method7)customAttackMethod7,
    (game::IBatAttackVftable::IsImmune)customAttackIsImmune,
    (game::IBatAttackVftable::OnAttack)customAttackOnMiss,
    (game::IBatAttackVftable::GetAttackClass)customAttackGetAttackClass,
    (game::IBatAttackVftable::GetAttackClass)customAttackGetUnderlyingAttackClass,
    (game::IBatAttackVftable::OnAttack)customAttackDoAttack,
    (game::IBatAttackVftable::Method13)customAttackMethod13,
    (game::IBatAttackVftable::UnknownMethod)customAttackMethod14,
    (game::IBatAttackVftable::UnknownMethod)customAttackMethod15,
    (game::IBatAttackVftable::UnknownMethod)customAttackMethod16,
    (game::IBatAttackVftable::UnknownMethod)customAttackMethod17,
};
// clang-format on

game::IBatAttack* customAttackCtor(CustomAttack* attack,
                                   game::IMidgardObjectMap* objectMap,
                                   const game::CMidgardID* id1,
                                   const game::CMidgardID* id2,
                                   int attackNumber)
{
    spdlog::debug("CustomAttack c-tor");
    attack->id1 = *id1;
    attack->id2 = *id2;
    attack->attackNumber = attackNumber;
    attack->attack = game::gameFunctions().getAttackById(objectMap, id1, attackNumber, true);
    attack->vftable = &customAttackVftable;
    return attack;
}

} // namespace hooks
