/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2024 Vladimir Makeev.
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

#include "mercsview.h"
#include "midsitemercs.h"
#include "midgardobjectmap.h"
#include "unitutils.h"
#include "usunitimpl.h"
#include <sol/sol.hpp>
#include <game.h>
#include <ussoldier.h>
#include <unitgenerator.h>
#include <mempool.h>

namespace bindings {

MercenaryUnitView::MercenaryUnitView(game::MercenaryUnit* mercUnit)
    : mercUnit{mercUnit}
{ }

void MercenaryUnitView::bind(sol::state& lua)
{
    auto view = lua.new_usertype<MercenaryUnitView>("MercenaryUnitView");

    view["impl"] = sol::property(&MercenaryUnitView::getImpl);
    view["unique"] = sol::property(&MercenaryUnitView::isUnique);
    view["level"] = sol::property(&MercenaryUnitView::getLevel);
    view["SetUnique"] = &MercenaryUnitView::setUnique;
}

UnitImplView MercenaryUnitView::getImpl() const
{
    return UnitImplView{hooks::getUnitImpl(&mercUnit->unitId)};
}

bool MercenaryUnitView::isUnique() const
{
    return mercUnit->unique;
}

int MercenaryUnitView::getLevel() const
{
    using namespace game;

    const auto* impl = hooks::getUnitImpl(&mercUnit->unitId);
    if (!impl) {
        return 0;
    }

    auto soldier = gameFunctions().castUnitImplToSoldier(impl);
    if (!soldier) {
        return 0;
    }

    return soldier->vftable->getLevel(soldier);
}

void MercenaryUnitView::setUnique(bool value)
{
    mercUnit->unique = value;
}

MercsView::MercsView(const game::CMidSiteMercs* mercs, const game::IMidgardObjectMap* objectMap)
    : SiteView(mercs, objectMap)
{ }

void MercsView::bind(sol::state& lua)
{
    auto view = lua.new_usertype<MercsView>("MercsView", sol::base_classes, sol::bases<SiteView>());

    bindAccessMethods(view);

    view["units"] = sol::property(&MercsView::getUnits);

    view["AddUnit"] = sol::overload(&MercsView::addUnit, &MercsView::addUnitByString,
        [](MercsView& self, const IdView& id, int level) { return self.addUnit(id, level, false); },
        [](MercsView& self, const std::string& id, int level) { return self.addUnitByString(id, level, false); },
        [](MercsView& self, const IdView& id) { return self.addUnit(id, 0, false); },
        [](MercsView& self, const std::string& id) { return self.addUnitByString(id, 0, false); });

    view["RemoveUnit"] = sol::overload(&MercsView::removeUnit, &MercsView::removeUnitById,
        [](MercsView& self, const IdView& id) { return self.removeUnit(id, 0); },
        [](MercsView& self, const std::string& id) { return self.removeUnitById(id, 0); });
}

std::vector<MercenaryUnitView> MercsView::getUnits() const
{
    auto* mercs = const_cast<game::CMidSiteMercs*>(static_cast<const game::CMidSiteMercs*>(site));

    std::vector<MercenaryUnitView> units;
    units.reserve(mercs->units.length);

    for (auto& mercUnit : mercs->units) {
        units.emplace_back(&mercUnit);
    }
    return units;
}

bool MercsView::addUnit(const IdView& unitImplId, int level, bool unique)
{
    using namespace game;

    const auto& globalApi = GlobalDataApi::get();
    GlobalData* globalData = *globalApi.getGlobalData();
    CUnitGenerator* generator = globalData->unitGenerator;

    auto* obj = const_cast<IMidgardObjectMap*>(objectMap);
    CMidSiteMercs* merc = static_cast<CMidSiteMercs*>(
        obj->vftable->findScenarioObjectByIdForChange(obj, &site->id));

    CMidgardID implId = unitImplId.id;
    if (level > 0) {
        CMidgardID genId{};
        int lvl = std::min(level, hooks::getGeneratedUnitImplLevelMax());
        generator->vftable->generateUnitImplId(generator, &genId, &implId, lvl);
        if (genId != implId && generator->vftable->isUnitGenerated(generator, &genId)) {
            if (generator->vftable->generateUnitImpl(generator, &genId)) {
                implId = genId;
            }
        }
    }

    for (auto& existing : merc->units) {
        if (existing.unitId != implId) {
            continue;
        }
        if (existing.unique != unique) {
            existing.unique = unique;
            return true;
        }
        return false;
    }

    void* raw = nullptr;
    const size_t nodeSize = sizeof(ListNode<MercenaryUnit>);

    if (auto allocFn = game::Memory::get().allocate) {
        raw = allocFn(static_cast<int>(nodeSize));
    } else {
        raw = std::malloc(nodeSize);
    }

    auto* newNode = static_cast<ListNode<MercenaryUnit>*>(raw);
    if (!newNode)
        return false;
    new (&newNode->data) MercenaryUnit{implId, unique};

    auto* head = merc->units.head;
    newNode->next = head;
    newNode->prev = head->prev;
    head->prev->next = newNode;
    head->prev = newNode;
    merc->units.length++;

    return true;
}

bool MercsView::addUnitByString(const std::string& id, int level, bool unique)
{
    return addUnit(IdView{id}, level, unique);
}

bool MercsView::removeUnit(const IdView& unitImplId, int level)
{
    using namespace game;

    auto* obj = const_cast<IMidgardObjectMap*>(objectMap);
    CMidSiteMercs* mercs = static_cast<CMidSiteMercs*>(
        obj->vftable->findScenarioObjectByIdForChange(obj, &site->id));

    if (!mercs)
        return false;

    CUnitGenerator* unitGenerator = (*(GlobalDataApi::get().getGlobalData()))->unitGenerator;

    CMidgardID searchId{};
    if (level > 0) {
        int lvl = std::min(level, hooks::getGeneratedUnitImplLevelMax());
        unitGenerator->vftable->generateUnitImplId(unitGenerator, &searchId, &unitImplId.id, lvl);
    }

    for (auto* node = mercs->units.head->next; node != mercs->units.head; node = node->next) {
        if (level > 0) {
            if (node->data.unitId != searchId)
                continue;
        } else {
            CMidgardID baseId{};
            unitGenerator->vftable->getGlobalUnitImplId(unitGenerator, &baseId, &node->data.unitId);
            if (baseId != unitImplId.id)
                continue;
        }

        node->prev->next = node->next;
        node->next->prev = node->prev;
        --mercs->units.length;

        if (auto freeFn = Memory::get().freeNonZero) {
            freeFn(node);
        } else {
            std::free(node);
        }

        return true;
    }

    return false;
}

bool MercsView::removeUnitById(const std::string& id, int level)
{
    return removeUnit(IdView{id}, level);
}

} // namespace bindings
