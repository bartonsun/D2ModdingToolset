/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2022 Vladimir Makeev.
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

#include "groupview.h"
#include "fortification.h"
#include "game.h"
#include "gameutils.h"
#include "idview.h"
#include "midgardobjectmap.h"
#include "midruin.h"
#include "midstack.h"
#include "midsubrace.h"
#include "midunitgroup.h"
#include "unitslotview.h"
#include "unitview.h"
#include "visitors.h"
#include <sol/sol.hpp>
#include <variant>

#include <unitutils.h>
#include <ussoldier.h>
#include <usunit.h>
#include <unitgenerator.h>
#include <scenarioinfo.h>
#include <midunit.h>

namespace bindings {

GroupView::GroupView(const game::CMidUnitGroup* group,
                     const game::IMidgardObjectMap* objectMap,
                     const game::CMidgardID* groupId)
    : group{group}
    , objectMap{objectMap}
    , groupId{*groupId}
{ }

void GroupView::bind(sol::state& lua)
{
    auto group = lua.new_usertype<GroupView>("GroupView");
    group["id"] = sol::property(&GroupView::getId);
    group["slots"] = sol::property(&GroupView::getSlots);
    group["units"] = sol::property(&GroupView::getUnits);
    group["hasUnit"] = sol::overload<>(&GroupView::hasUnit, &GroupView::hasUnitById);
    group["getUnitPosition"] = sol::overload<>(&GroupView::getUnitPosition,
                                               &GroupView::getUnitPositionById);
    group["subrace"] = sol::property(&GroupView::getSubrace);
    group["AddUnit"] = &GroupView::addUnit;
    group["RemoveUnit"] = &GroupView::removeUnit;
    ///[](MercsView& self, const IdView& id, int level) { return self.addUnit(id, level, false); },
    group["SwapSlots"] = sol::overload(&GroupView::moveUnitInGroup, 
        [](GroupView& self, int fromSlot, int toSlot) { return self.moveUnitInGroup(fromSlot, toSlot, self.groupId);
    });
}

IdView GroupView::getId() const
{
    return IdView{groupId};
}

GroupView::GroupSlots GroupView::getSlots() const
{
    const auto& fn = game::gameFunctions();
    GroupSlots slots;

    for (size_t i = 0; i < std::size(group->positions); ++i) {
        const auto& unitId{group->positions[i]};

        if (unitId == game::emptyId) {
            slots.emplace_back(UnitSlotView(nullptr, i, &groupId));
            continue;
        }

        // Unit can be null in case where a map is in a state of loading, group object is already
        // serialized, while some (or all) of its units are not (yet).
        auto unit = fn.findUnitById(objectMap, &unitId);
        if (unit) {
            slots.emplace_back(UnitSlotView(unit, i, &groupId));
        }
    }

    return slots;
}

GroupView::GroupUnits GroupView::getUnits() const
{
    const auto& fn = game::gameFunctions();
    GroupUnits units;

    for (const game::CMidgardID* it = group->units.bgn; it != group->units.end; ++it) {
        auto unit = fn.findUnitById(objectMap, it);
        if (unit) {
            units.emplace_back(UnitView(unit));
        }
    }

    return units;
}

bool GroupView::hasUnit(const bindings::UnitView& unit) const
{
    auto unitId = unit.getId();
    return hasUnitById(unitId);
}

bool GroupView::hasUnitById(const bindings::IdView& unitId) const
{
    auto& units = group->units;
    for (const game::CMidgardID* it = units.bgn; it != units.end; ++it) {
        if (unitId.id == *it)
            return true;
    }

    return false;
}

int GroupView::getUnitPosition(const bindings::UnitView& unit) const
{
    return getUnitPositionById(unit.getId());
}

int GroupView::getUnitPositionById(const bindings::IdView& unitId) const
{
    return game::CMidUnitGroupApi::get().getUnitPosition(group, &unitId.id);
}

int GroupView::getSubrace() const
{
    using namespace game;

    const auto type = CMidgardIDApi::get().getType(&groupId);
    switch (type) {
    case IdType::Stack: {
        const CMidStack* stack{hooks::getStack(objectMap, &groupId)};

        auto obj{objectMap->vftable->findScenarioObjectById(objectMap, &stack->subraceId)};
        const CMidSubRace* subrace = static_cast<const game::CMidSubRace*>(obj);
        return static_cast<int>(subrace->subraceCategory.id);
    }

    case IdType::Fortification: {
        const CFortification* fort{hooks::getFort(objectMap, &groupId)};

        auto obj{objectMap->vftable->findScenarioObjectById(objectMap, &fort->subraceId)};
        const CMidSubRace* subrace = static_cast<const game::CMidSubRace*>(obj);
        return static_cast<int>(subrace->subraceCategory.id);
    }
    }

    return game::emptyCategoryId;
}

std::optional<UnitView> GroupView::addUnit(const IdView& unitImplId, int position, int level)
{ 
    using namespace game;

    const auto& globalApi = GlobalDataApi::get();
    const auto globalData = *globalApi.getGlobalData();
    static const auto& visitor = VisitorApi::get();

    IMidgardObjectMap* obj = const_cast<IMidgardObjectMap*>(objectMap);

    IUsUnit* unit = (IUsUnit*)globalApi.findById(globalData->units, &unitImplId.id);
    IUsSoldier* soldier = gameFunctions().castUnitImplToSoldier(unit);
    bool isSmall = soldier->vftable->getSizeSmall(soldier);

    int pos = position;
    auto positions = group->positions;

    if (pos < 0) {
        if (!isSmall) {
            for (int bigSlot : {0, 2, 4}) {
                if (positions[bigSlot] == emptyId && positions[bigSlot + 1] == emptyId) {
                    pos = bigSlot;
                    break;
                }
            }
        } else {
            for (int i = 0; i < 6; i++) {
                if (positions[i] == emptyId) {
                    pos = i;
                    break;
                }
            }
        }
        if (pos < 0)
            return std::nullopt;
    } else {
        if (isSmall) {
            if (positions[pos] != emptyId)
                return std::nullopt;
        } else {
            if (pos % 2 != 0) {
                pos = pos - 1;
            }
            if (pos > 4 || positions[pos] != emptyId || positions[pos + 1] != emptyId) {
                return std::nullopt;
            }
        }
    }

    CMidgardID implId = unitImplId.id;
    if (level > 0) {
        CUnitGenerator* unitGenerator = globalData->unitGenerator;
        CMidgardID genId{};
        int lvl = std::min(level, hooks::getGeneratedUnitImplLevelMax());
        unitGenerator->vftable->generateUnitImplId(unitGenerator, &genId, &implId, lvl);
        if (genId != implId && unitGenerator->vftable->isUnitGenerated(unitGenerator, &genId)) {
            if (unitGenerator->vftable->generateUnitImpl(unitGenerator, &genId)) {
                implId = genId;
            }
        }
    }

    int creationTurn = hooks::getScenarioInfo(objectMap)->currentTurn;

    if (!visitor.addUnitToGroup(&implId, &groupId, pos, &creationTurn, 1, obj, 1))
        return std::nullopt;

    return UnitView(game::gameFunctions().findUnitById(objectMap, &group->positions[pos]));
}

bool GroupView::removeUnit(const IdView& unit)
{ 
    using namespace game;

    static const auto& visitor = VisitorApi::get();
    IMidgardObjectMap* obj = const_cast<IMidgardObjectMap*>(objectMap);

    const auto type = CMidgardIDApi::get().getType(&groupId);

    if (type != IdType::Stack && type != IdType::Fortification && type != IdType::Ruin)
        return false;
    
    if (type == IdType::Stack)
    {
        CMidStack* stack = hooks::getStack(objectMap, &groupId);
        if (stack->leaderId == unit.id)
            return false;
    }

    return visitor.extractUnitFromGroup(&unit.id, &groupId, obj, 1);
}

bool GroupView::moveUnitInGroup(int fromSlot, int toSlot, const IdView& toGroupId)
{
    using namespace game;

    static const auto& visitor = VisitorApi::get();
    const auto& idApi = CMidgardIDApi::get();
    const auto& unitGroupApi = CMidUnitGroupApi::get();

    IMidgardObjectMap* obj = const_cast<IMidgardObjectMap*>(objectMap);

    if (visitor.swapUnitPosition(fromSlot, &groupId, toSlot, &toGroupId.id, obj, 0))
        return visitor.swapUnitPosition(fromSlot, &groupId, toSlot, &toGroupId.id, obj, 1);
    return visitor.swapUnitPosition(toSlot, &toGroupId.id, fromSlot, &groupId, obj, 1);
}

} // namespace bindings
