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

#include "scenarioview.h"
#include "crystalview.h"
#include "diplomacyview.h"
#include "dynamiccast.h"
#include "fortification.h"
#include "fortview.h"
#include "gameutils.h"
#include "idview.h"
#include "itemview.h"
#include "landmarkview.h"
#include "locationview.h"
#include "merchantview.h"
#include "mercsview.h"
#include "midcrystal.h"
#include "midgardmapblock.h"
#include "midgardobjectmap.h"
#include "midgardplan.h"
#include "midlandmark.h"
#include "midplayer.h"
#include "midrod.h"
#include "midruin.h"
#include "midscenvariables.h"
#include "midsitemerchant.h"
#include "midsitemercs.h"
#include "midsiteresourcemarket.h"
#include "midsitetrainer.h"
#include "midunit.h"
#include "playerview.h"
#include "point.h"
#include "resourcemarketview.h"
#include "rodview.h"
#include "ruinview.h"
#include "scenarioinfo.h"
#include "scenvariablesview.h"
#include "sitecategoryhooks.h"
#include "stackview.h"
#include "tileview.h"
#include "trainerview.h"
#include "unitview.h"
#include <sol/sol.hpp>

#include <game.h>
#include <modifierutils.h>
#include <unitutils.h>
#include <visitors.h>
#include "usunitimpl.h"
#include "ussoldier.h"
#include <hooks.h>
#include <usstackleader.h>
#include <midstack.h>

#include <interfmanager.h>
#include <dialoginterf.h>
#include <battleviewerinterf.h>
#include <batlogichooks.h>
#include <battleviewerinterfhooks.h>
#include <batattackutils.h>
#include <bagview.h>
#include <midbag.h>
#include <magetowerview.h>
#include <midsitemage.h>

#include "plandata.h"
#include "mqpointlist.h"
#include "categoryids.h"
#include "racetype.h"
#include "version.h"
#include <array>

#include <idset.h>
#include <midserverlogic.h>
#include <midgard.h>
#include <midserver.h>
#include <midclient.h>
#include <midgardscenariomap.h>

namespace bindings {

ScenarioView::ScenarioView(const game::IMidgardObjectMap* objectMap)
    : objectMap(objectMap)
{ }

void ScenarioView::bind(sol::state& lua)
{
    auto scenario = lua.new_usertype<ScenarioView>("Scenario");
    scenario["getLocation"] = sol::overload<>(&ScenarioView::getLocation,
                                              &ScenarioView::getLocationById);
    scenario["variables"] = sol::property(&ScenarioView::getScenVariables);
    scenario["getTile"] = sol::overload<>(&ScenarioView::getTile, &ScenarioView::getTileByPoint);
    scenario["getStack"] = sol::overload<>(&ScenarioView::getStack, &ScenarioView::getStackById,
                                           &ScenarioView::getStackByCoordinates,
                                           &ScenarioView::getStackByPoint);
    scenario["getFort"] = sol::overload<>(&ScenarioView::getFort, &ScenarioView::getFortById,
                                          &ScenarioView::getFortByCoordinates,
                                          &ScenarioView::getFortByPoint);
    scenario["getRuin"] = sol::overload<>(&ScenarioView::getRuin, &ScenarioView::getRuinById,
                                          &ScenarioView::getRuinByCoordinates,
                                          &ScenarioView::getRuinByPoint);
    scenario["getRod"] = sol::overload<>(&ScenarioView::getRod, &ScenarioView::getRodById,
                                         &ScenarioView::getRodByCoordinates,
                                         &ScenarioView::getRodByPoint);
    scenario["getPlayer"] = sol::overload<>(&ScenarioView::getPlayer, &ScenarioView::getPlayerById);
    scenario["getUnit"] = sol::overload<>(&ScenarioView::getUnit, &ScenarioView::getUnitById);
    scenario["getItem"] = sol::overload<>(&ScenarioView::getItem, &ScenarioView::getItemById);
    scenario["getCrystal"] = sol::overload<>(&ScenarioView::getCrystal,
                                             &ScenarioView::getCrystalById,
                                             &ScenarioView::getCrystalByCoordinates,
                                             &ScenarioView::getCrystalByPoint);
    scenario["getMerchant"] = sol::overload<>(&ScenarioView::getMerchant,
                                              &ScenarioView::getMerchantById,
                                              &ScenarioView::getMerchantByCoordinates,
                                              &ScenarioView::getMerchantByPoint);
    scenario["getMercenary"] = sol::overload<>(&ScenarioView::getMercs, &ScenarioView::getMercsById,
                                               &ScenarioView::getMercsByCoordinates,
                                               &ScenarioView::getMercsByPoint);
    scenario["getTrainer"] = sol::overload<>(&ScenarioView::getTrainer,
                                             &ScenarioView::getTrainerById,
                                             &ScenarioView::getTrainerByCoordinates,
                                             &ScenarioView::getTrainerByPoint);
    scenario["getMarket"] = sol::overload<>(&ScenarioView::getMarket, &ScenarioView::getMarketById,
                                            &ScenarioView::getMarketByCoordinates,
                                            &ScenarioView::getMarketByPoint);

    scenario["findStackByUnit"] = sol::overload<>(&ScenarioView::findStackByUnit,
                                                  &ScenarioView::findStackByUnitId,
                                                  &ScenarioView::findStackByUnitIdString);
    scenario["findFortByUnit"] = sol::overload<>(&ScenarioView::findFortByUnit,
                                                 &ScenarioView::findFortByUnitId,
                                                 &ScenarioView::findFortByUnitIdString);
    scenario["findRuinByUnit"] = sol::overload<>(&ScenarioView::findRuinByUnit,
                                                 &ScenarioView::findRuinByUnitId,
                                                 &ScenarioView::findRuinByUnitIdString);
    scenario["getLandmark"] = sol::overload(&ScenarioView::getLandmark,
                                            &ScenarioView::getLandmarkById,
                                            &ScenarioView::getLandmarkByCoordinates,
                                            &ScenarioView::getLandmarkByPoint);
    scenario["name"] = sol::property(&ScenarioView::getName);
    scenario["description"] = sol::property(&ScenarioView::getDescription);
    scenario["author"] = sol::property(&ScenarioView::getAuthor);
    scenario["seed"] = sol::property(&ScenarioView::getSeed);
    scenario["day"] = sol::property(&ScenarioView::getCurrentDay);
    scenario["size"] = sol::property(&ScenarioView::getSize);
    scenario["difficulty"] = sol::property(&ScenarioView::getDifficulty);
    scenario["diplomacy"] = sol::property(&ScenarioView::getDiplomacy);
    scenario["forEachStack"] = &ScenarioView::forEachStack;
    scenario["forEachLocation"] = &ScenarioView::forEachLocation;
    scenario["forEachFort"] = &ScenarioView::forEachFort;
    scenario["forEachRuin"] = &ScenarioView::forEachRuin;
    scenario["forEachRod"] = &ScenarioView::forEachRod;
    scenario["forEachPlayer"] = &ScenarioView::forEachPlayer;
    scenario["forEachUnit"] = &ScenarioView::forEachUnit;
    scenario["forEachCrystal"] = &ScenarioView::forEachCrystal;
    scenario["forEachMerchant"] = &ScenarioView::forEachMerchant;
    scenario["forEachMercenary"] = &ScenarioView::forEachMercenary;
    scenario["forEachTrainer"] = &ScenarioView::forEachTrainer;
    scenario["forEachMarket"] = &ScenarioView::forEachMarket;
    scenario["forEachLandmark"] = &ScenarioView::forEachLandmark;
    scenario["forEachBag"] = &ScenarioView::forEachBag;
    scenario["forEachMageTower"] = &ScenarioView::forEachMageTower;

    scenario["AddUnitXP"] = sol::overload<>(&ScenarioView::addUnitXP);
    scenario["Heal"] = sol::overload<>(&ScenarioView::heal);
    scenario["SetHealth"] = sol::overload<>(&ScenarioView::setHealth);
    scenario["AddUnitModifier"] = sol::overload<>(&ScenarioView::addUnitModifier);
    scenario["RemoveUnitModifier"] = sol::overload<>(&ScenarioView::removeUnitModifier);
    scenario["SetTransform"] = sol::overload<>(&ScenarioView::setTransform);
    scenario["addUnitXpWithUpgrade"] = sol::overload<>(&ScenarioView::addUnitXpWithUpgrade);
    scenario["GiveSkillPoint"] = &ScenarioView::giveSkillPoint;
    scenario["CreateRod"] = &ScenarioView::createRod;
    scenario["MoveStack"] = &ScenarioView::moveStack;
    scenario["ChangeTerrain"] = &ScenarioView::mapChangeTerrain;

    scenario["getBag"] = sol::overload<>(&ScenarioView::getBag, &ScenarioView::getBagById,
                                           &ScenarioView::getBagByCoordinates,
                                           &ScenarioView::getBagByPoint);
    scenario["getMageTower"] = sol::overload<>(&ScenarioView::getMageTower,
                                               &ScenarioView::getMageTowerById,
                                               &ScenarioView::getMageTowerByCoordinates,
                                               &ScenarioView::getMageTowerByPoint);
    scenario["SetVariable"] = sol::overload<>(&ScenarioView::setVariableByName,
                                              &ScenarioView::setVariableById);
}

std::optional<LocationView> ScenarioView::getLocation(const std::string& id) const
{
    return getLocationById(IdView{id});
}

std::optional<LocationView> ScenarioView::getLocationById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto* location = (const game::CMidLocation*)dynamicCast(obj, 0, rtti.IMidScenarioObjectType,
                                                            rtti.CMidLocationType, 0);
    if (!location) {
        return std::nullopt;
    }

    return LocationView{location};
}

std::optional<ScenVariablesView> ScenarioView::getScenVariables() const
{
    if (!objectMap) {
        return std::nullopt;
    }

    auto variables{hooks::getScenarioVariables(objectMap)};
    if (!variables) {
        return std::nullopt;
    }

    return ScenVariablesView{variables};
}

std::optional<TileView> ScenarioView::getTile(int x, int y) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    auto info = hooks::getScenarioInfo(objectMap);
    if (!info) {
        return std::nullopt;
    }

    auto block = hooks::getMidgardMapBlock(objectMap, &info->id, info->mapSize, x, y);
    if (!block) {
        return std::nullopt;
    }

    const auto& blockPos = block->position;
    if (x < blockPos.x || x >= blockPos.x + 8 || y < blockPos.y || y >= blockPos.y + 4) {
        // Outside of map block
        return std::nullopt;
    }

    const auto index = x + 8 * (y - blockPos.y) - blockPos.x;
    if (index < 0 || index >= 32) {
        return std::nullopt;
    }

    return TileView{block->tiles[index]};
}

std::optional<TileView> ScenarioView::getTileByPoint(const Point& p) const
{
    return getTile(p.x, p.y);
}

std::optional<StackView> ScenarioView::getStack(const std::string& id) const
{
    return getStackById(IdView{id});
}

std::optional<StackView> ScenarioView::getStackById(const IdView& id) const
{
    if (!objectMap) {
        return std::nullopt;
    }

    auto stack = hooks::getStack(objectMap, &id.id);
    if (!stack) {
        return std::nullopt;
    }

    return StackView{stack, objectMap};
}

std::optional<StackView> ScenarioView::getStackByCoordinates(int x, int y) const
{
    auto stackId = getObjectId(x, y, game::IdType::Stack);
    if (!stackId) {
        return std::nullopt;
    }

    return getStackById(IdView{stackId});
}

std::optional<StackView> ScenarioView::getStackByPoint(const Point& p) const
{
    return getStackByCoordinates(p.x, p.y);
}

std::optional<StackView> ScenarioView::findStackByUnit(const UnitView& unit) const
{
    auto unitId = unit.getId();
    return findStackByUnitId(unitId);
}

std::optional<StackView> ScenarioView::findStackByUnitId(const IdView& unitId) const
{
    if (!objectMap) {
        return std::nullopt;
    }

    auto stack = hooks::getStackByUnitId(objectMap, &unitId.id);
    if (!stack) {
        return std::nullopt;
    }

    return {StackView{stack, objectMap}};
}

std::optional<StackView> ScenarioView::findStackByUnitIdString(const std::string& unitId) const
{
    return findStackByUnitId(IdView{unitId});
}

std::optional<FortView> ScenarioView::getFort(const std::string& id) const
{
    return getFortById(IdView{id});
}

std::optional<FortView> ScenarioView::getFortById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Fortification) {
        return std::nullopt;
    }

    auto fort = hooks::getFort(objectMap, &id.id);
    if (!fort) {
        return std::nullopt;
    }

    return {FortView{fort, objectMap}};
}

std::optional<FortView> ScenarioView::getFortByCoordinates(int x, int y) const
{
    auto fortId = getObjectId(x, y, game::IdType::Fortification);
    if (!fortId) {
        return std::nullopt;
    }

    return getFortById(IdView{fortId});
}

std::optional<FortView> ScenarioView::getFortByPoint(const Point& p) const
{
    return getFortByCoordinates(p.x, p.y);
}

std::optional<FortView> ScenarioView::findFortByUnit(const UnitView& unit) const
{
    auto unitId = unit.getId();
    return findFortByUnitId(unitId);
}

std::optional<FortView> ScenarioView::findFortByUnitId(const IdView& unitId) const
{
    if (!objectMap) {
        return std::nullopt;
    }

    auto fort = hooks::getFortByUnitId(objectMap, &unitId.id);
    if (!fort) {
        return std::nullopt;
    }

    return {FortView{fort, objectMap}};
}

std::optional<FortView> ScenarioView::findFortByUnitIdString(const std::string& unitId) const
{
    return findFortByUnitId(IdView{unitId});
}

std::optional<RuinView> ScenarioView::getRuin(const std::string& id) const
{
    return getRuinById(IdView{id});
}

std::optional<RuinView> ScenarioView::getRuinById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Ruin) {
        return std::nullopt;
    }

    auto ruin = hooks::getRuin(objectMap, &id.id);
    if (!ruin) {
        return std::nullopt;
    }

    return {RuinView{ruin, objectMap}};
}

std::optional<RuinView> ScenarioView::getRuinByCoordinates(int x, int y) const
{
    auto ruinId = getObjectId(x, y, game::IdType::Ruin);
    if (!ruinId) {
        return std::nullopt;
    }

    return getRuinById(IdView{ruinId});
}

std::optional<RuinView> ScenarioView::getRuinByPoint(const Point& p) const
{
    return getRuinByCoordinates(p.x, p.y);
}

std::optional<RodView> ScenarioView::getRod(const std::string& id) const
{
    return getRodById(IdView{id});
}

std::optional<RodView> ScenarioView::getRodById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Rod) {
        return std::nullopt;
    }

    auto rod = hooks::getRod(objectMap, &id.id);
    if (!rod) {
        return std::nullopt;
    }

    return {RodView{rod, objectMap}};
}

std::optional<RodView> ScenarioView::getRodByCoordinates(int x, int y) const
{
    auto rodId = getObjectId(x, y, game::IdType::Rod);
    if (!rodId) {
        return std::nullopt;
    }

    return getRodById(IdView{rodId});
}

std::optional<RodView> ScenarioView::getRodByPoint(const Point& p) const
{
    return getRodByCoordinates(p.x, p.y);
}

std::optional<RuinView> ScenarioView::findRuinByUnit(const UnitView& unit) const
{
    auto unitId = unit.getId();
    return findRuinByUnitId(unitId);
}

std::optional<RuinView> ScenarioView::findRuinByUnitId(const IdView& unitId) const
{
    if (!objectMap) {
        return std::nullopt;
    }

    auto ruin = hooks::getRuinByUnitId(objectMap, &unitId.id);
    if (!ruin) {
        return std::nullopt;
    }

    return {RuinView{ruin, objectMap}};
}

std::optional<RuinView> ScenarioView::findRuinByUnitIdString(const std::string& unitId) const
{
    return findRuinByUnitId(IdView{unitId});
}

std::optional<PlayerView> ScenarioView::getPlayer(const std::string& id) const
{
    return getPlayerById(IdView{id});
}

std::optional<PlayerView> ScenarioView::getPlayerById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Player) {
        return std::nullopt;
    }

    auto player = hooks::getPlayer(objectMap, &id.id);
    if (!player) {
        return std::nullopt;
    }

    return {PlayerView{player, objectMap}};
}

std::optional<UnitView> ScenarioView::getUnit(const std::string& id) const
{
    return getUnitById(IdView{id});
}

std::optional<UnitView> ScenarioView::getUnitById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Unit) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    return {UnitView{(const CMidUnit*)obj}};
}

std::optional<ItemView> ScenarioView::getItem(const std::string& id) const
{
    return getItemById(IdView{id});
}

std::optional<ItemView> ScenarioView::getItemById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Item) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    return {ItemView{&id.id, objectMap}};
}

std::optional<CrystalView> ScenarioView::getCrystal(const std::string& id) const
{
    return getCrystalById(IdView{id});
}

std::optional<CrystalView> ScenarioView::getCrystalById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Crystal) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    return {CrystalView{static_cast<const CMidCrystal*>(obj)}};
}

std::optional<CrystalView> ScenarioView::getCrystalByCoordinates(int x, int y) const
{
    auto crystalId = getObjectId(x, y, game::IdType::Crystal);
    if (!crystalId) {
        return std::nullopt;
    }

    return getCrystalById(IdView{crystalId});
}

std::optional<CrystalView> ScenarioView::getCrystalByPoint(const Point& p) const
{
    return getCrystalByCoordinates(p.x, p.y);
}

std::optional<MerchantView> ScenarioView::getMerchant(const std::string& id) const
{
    return getMerchantById(IdView{id});
}

std::optional<MerchantView> ScenarioView::getMerchantById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Site) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    auto site = static_cast<const CMidSite*>(obj);
    if (site->siteCategory.id != SiteCategories::get().merchant->id) {
        return std::nullopt;
    }

    return MerchantView{static_cast<const CMidSiteMerchant*>(site), objectMap};
}

std::optional<MerchantView> ScenarioView::getMerchantByCoordinates(int x, int y) const
{
    auto merchantId = getObjectId(x, y, game::IdType::Site);
    if (!merchantId) {
        return std::nullopt;
    }

    return getMerchantById(IdView{merchantId});
}

std::optional<MerchantView> ScenarioView::getMerchantByPoint(const Point& p) const
{
    return getMerchantByCoordinates(p.x, p.y);
}

std::optional<MercsView> ScenarioView::getMercs(const std::string& id) const
{
    return getMercsById(IdView{id});
}

std::optional<MercsView> ScenarioView::getMercsById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Site) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    auto site = static_cast<const CMidSite*>(obj);
    if (site->siteCategory.id != SiteCategories::get().mercenaries->id) {
        return std::nullopt;
    }

    return MercsView{static_cast<const CMidSiteMercs*>(site), objectMap};
}

std::optional<MercsView> ScenarioView::getMercsByCoordinates(int x, int y) const
{
    auto mercenariesId = getObjectId(x, y, game::IdType::Site);
    if (!mercenariesId) {
        return std::nullopt;
    }

    return getMercsById(IdView{mercenariesId});
}

std::optional<MercsView> ScenarioView::getMercsByPoint(const Point& p) const
{
    return getMercsByCoordinates(p.x, p.y);
}

std::optional<TrainerView> ScenarioView::getTrainer(const std::string& id) const
{
    return getTrainerById(IdView{id});
}

std::optional<TrainerView> ScenarioView::getTrainerById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Site) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    auto site = static_cast<const CMidSite*>(obj);
    if (site->siteCategory.id != SiteCategories::get().trainer->id) {
        return std::nullopt;
    }

    return TrainerView{static_cast<const CMidSiteTrainer*>(site), objectMap};
}

std::optional<TrainerView> ScenarioView::getTrainerByCoordinates(int x, int y) const
{
    auto trainerId = getObjectId(x, y, game::IdType::Site);
    if (!trainerId) {
        return std::nullopt;
    }

    return getTrainerById(IdView{trainerId});
}

std::optional<TrainerView> ScenarioView::getTrainerByPoint(const Point& p) const
{
    return getTrainerByCoordinates(p.x, p.y);
}

std::optional<ResourceMarketView> ScenarioView::getMarket(const std::string& id) const
{
    return getMarketById(IdView{id});
}

std::optional<ResourceMarketView> ScenarioView::getMarketById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Site) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    auto site = static_cast<const CMidSite*>(obj);
    if (site->siteCategory.id != hooks::customSiteCategories().resourceMarket.id) {
        return std::nullopt;
    }

    return ResourceMarketView{static_cast<const hooks::CMidSiteResourceMarket*>(site), objectMap};
}

std::optional<ResourceMarketView> ScenarioView::getMarketByCoordinates(int x, int y) const
{
    auto marketId = getObjectId(x, y, game::IdType::Site);
    if (!marketId) {
        return std::nullopt;
    }

    return getMarketById(IdView{marketId});
}

std::optional<ResourceMarketView> ScenarioView::getMarketByPoint(const Point& p) const
{
    return getMarketByCoordinates(p.x, p.y);
}

std::optional<LandmarkView> ScenarioView::getLandmark(const std::string& id) const
{
    return getLandmarkById(IdView{id});
}

std::optional<LandmarkView> ScenarioView::getLandmarkById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Landmark) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    auto* landmark = static_cast<const CMidLandmark*>(obj);
    return LandmarkView{landmark, objectMap};
}

std::optional<LandmarkView> ScenarioView::getLandmarkByCoordinates(int x, int y) const
{
    auto landmarkId = getObjectId(x, y, game::IdType::Landmark);
    if (!landmarkId) {
        return std::nullopt;
    }

    return getLandmarkById(IdView{landmarkId});
}

std::optional<LandmarkView> ScenarioView::getLandmarkByPoint(const Point& p) const
{
    return getLandmarkByCoordinates(p.x, p.y);
}

void ScenarioView::forEachLandmark(const std::function<void(const LandmarkView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    auto runCallback = [this, &callback](const IMidScenarioObject* obj) {
        auto* landmark = static_cast<const CMidLandmark*>(obj);
        const LandmarkView landmarkView{landmark, objectMap};
        callback(landmarkView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Landmark, runCallback);
}

std::string ScenarioView::getName() const
{
    if (!objectMap) {
        return "";
    }

    auto info = hooks::getScenarioInfo(objectMap);
    return info->name ? info->name : "";
}

std::string ScenarioView::getDescription() const
{
    if (!objectMap) {
        return "";
    }

    auto info = hooks::getScenarioInfo(objectMap);
    return info->description ? info->description : "";
}

std::string ScenarioView::getAuthor() const
{
    if (!objectMap) {
        return "";
    }

    auto info = hooks::getScenarioInfo(objectMap);
    return info->creator ? info->creator : "";
}

std::uint32_t ScenarioView::getSeed() const
{
    if (!objectMap) {
        return 0u;
    }

    auto info = hooks::getScenarioInfo(objectMap);
    return static_cast<std::uint32_t>(info->mapSeed);
}

int ScenarioView::getCurrentDay() const
{
    if (!objectMap) {
        return 0;
    }

    auto info = hooks::getScenarioInfo(objectMap);
    return info ? info->currentTurn : 0;
}

int ScenarioView::getSize() const
{
    if (!objectMap) {
        return 0;
    }

    auto info = hooks::getScenarioInfo(objectMap);
    return info ? info->mapSize : 0;
}

int ScenarioView::getDifficulty() const
{
    if (!objectMap) {
        return game::emptyCategoryId;
    }

    auto info = hooks::getScenarioInfo(objectMap);
    return info ? static_cast<int>(info->gameDifficulty.id) : game::emptyCategoryId;
}

std::optional<DiplomacyView> ScenarioView::getDiplomacy() const
{
    if (!objectMap) {
        return std::nullopt;
    }

    return DiplomacyView{hooks::getDiplomacy(objectMap)};
}

void ScenarioView::forEachStack(const std::function<void(const StackView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto runCallback = [this, &callback, &dynamicCast, &rtti](const IMidScenarioObject* obj) {
        auto* stack = (const CMidStack*)dynamicCast(obj, 0, rtti.IMidScenarioObjectType,
                                                    rtti.CMidStackType, 0);

        const StackView stackView{stack, objectMap};
        callback(stackView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Stack, runCallback);
}

void ScenarioView::forEachLocation(const std::function<void(const LocationView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto runCallback = [&callback, &dynamicCast, &rtti](const IMidScenarioObject* obj) {
        auto* location = (const CMidLocation*)dynamicCast(obj, 0, rtti.IMidScenarioObjectType,
                                                          rtti.CMidLocationType, 0);

        const LocationView locationView{location};
        callback(locationView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Location, runCallback);
}

void ScenarioView::forEachFort(const std::function<void(const FortView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    auto runCallback = [this, &callback](const IMidScenarioObject* obj) {
        auto* fort{static_cast<const CFortification*>(obj)};

        const FortView fortView{fort, objectMap};
        callback(fortView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Fortification, runCallback);
}

void ScenarioView::forEachRuin(const std::function<void(const RuinView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    auto runCallback = [this, &callback](const IMidScenarioObject* obj) {
        auto* ruin{static_cast<const CMidRuin*>(obj)};

        const RuinView ruinView{ruin, objectMap};
        callback(ruinView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Ruin, runCallback);
}

void ScenarioView::forEachRod(const std::function<void(const RodView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    auto runCallback = [this, &callback](const IMidScenarioObject* obj) {
        auto* rod{static_cast<const CMidRod*>(obj)};

        const RodView rodView{rod, objectMap};
        callback(rodView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Rod, runCallback);
}

void ScenarioView::forEachPlayer(const std::function<void(const PlayerView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    auto runCallback = [this, &callback](const IMidScenarioObject* obj) {
        auto* player{static_cast<const CMidPlayer*>(obj)};

        const PlayerView playerView{player, objectMap};
        callback(playerView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Player, runCallback);
}

void ScenarioView::forEachUnit(const std::function<void(const UnitView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    auto runCallback = [&callback](const IMidScenarioObject* obj) {
        auto* unit{static_cast<const CMidUnit*>(obj)};

        const UnitView unitView{unit};
        callback(unitView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Unit, runCallback);
}

void ScenarioView::forEachCrystal(const std::function<void(const CrystalView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    auto runCallback = [&callback](const IMidScenarioObject* obj) {
        auto* crystal{static_cast<const CMidCrystal*>(obj)};

        const CrystalView crystalView{crystal};
        callback(crystalView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Crystal, runCallback);
}

void ScenarioView::forEachMerchant(const std::function<void(const MerchantView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    const auto merchantId{SiteCategories::get().merchant->id};

    auto runCallback = [this, &callback, &merchantId](const IMidScenarioObject* obj) {
        const auto* site{static_cast<const CMidSite*>(obj)};
        if (site->siteCategory.id != merchantId) {
            return;
        }

        const MerchantView view{static_cast<const CMidSiteMerchant*>(site), objectMap};
        callback(view);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Site, runCallback);
}

void ScenarioView::forEachMercenary(const std::function<void(const MercsView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    const auto mercsId{SiteCategories::get().mercenaries->id};

    auto runCallback = [this, &callback, &mercsId](const IMidScenarioObject* obj) {
        const auto* site{static_cast<const CMidSite*>(obj)};
        if (site->siteCategory.id != mercsId) {
            return;
        }

        const MercsView view{static_cast<const CMidSiteMercs*>(site), objectMap};
        callback(view);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Site, runCallback);
}

void ScenarioView::forEachTrainer(const std::function<void(const TrainerView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    const auto trainerId{SiteCategories::get().trainer->id};

    auto runCallback = [this, &callback, &trainerId](const IMidScenarioObject* obj) {
        const auto* site{static_cast<const CMidSite*>(obj)};
        if (site->siteCategory.id != trainerId) {
            return;
        }

        const TrainerView view{static_cast<const CMidSiteTrainer*>(site), objectMap};
        callback(view);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Site, runCallback);
}

void ScenarioView::forEachMarket(
    const std::function<void(const ResourceMarketView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    if (!hooks::customSiteCategories().exists) {
        return;
    }

    using namespace game;

    const auto marketId{hooks::customSiteCategories().resourceMarket.id};

    auto runCallback = [this, &callback, &marketId](const IMidScenarioObject* obj) {
        const auto* site{static_cast<const CMidSite*>(obj)};
        if (site->siteCategory.id != marketId) {
            return;
        }

        const ResourceMarketView view{static_cast<const hooks::CMidSiteResourceMarket*>(site),
                                      objectMap};
        callback(view);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Site, runCallback);
}

const game::CMidgardID* ScenarioView::getObjectId(int x, int y, game::IdType type) const
{
    using namespace game;

    if (!objectMap) {
        return nullptr;
    }

    auto plan{hooks::getMidgardPlan(objectMap)};
    if (!plan) {
        return nullptr;
    }

    const CMqPoint position{x, y};
    return CMidgardPlanApi::get().getObjectId(plan, &position, &type);
}
///////////////////////////////////////////////////////////////////////////////////////////////
int ScenarioView::addUnitXP(const IdView& unitId, int value)
{
    using namespace game;

    const auto& fn = gameFunctions();

    auto objMap = const_cast<game::IMidgardObjectMap*>(objectMap);
    auto unit = fn.findUnitById(objectMap, &unitId.id);

    if (unit == nullptr)
        return 0;

    int xpGain = hooks::addUnitXpNoUpgrade(objMap, unit, value);

    return xpGain;
}

bool ScenarioView::heal(const IdView& unitId, int value)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& visitors = VisitorApi::get();

    auto objMap = const_cast<game::IMidgardObjectMap*>(objectMap);
    auto unit = fn.findUnitById(objectMap, &unitId.id);

    if (unit == nullptr)
        return false;

    visitors.changeUnitHp(&unit->id, value, objMap, 1);

    return true;
}

bool ScenarioView::setHealth(const IdView& unitId, int value)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& visitors = VisitorApi::get();

    auto unit = fn.findUnitById(objectMap, &unitId.id);
    if (!unit)
        return false;

    value = std::clamp(value, 0, 9999);

    auto objMap = const_cast<game::IMidgardObjectMap*>(objectMap);

    int diff = value - unit->currentHp;

    visitors.changeUnitHp(&unit->id, diff, objMap, 1);

    return true;
}

bool ScenarioView::addUnitModifier(const IdView& unitId, const std::string& modifierId)
{
    using namespace game;
    auto& fn = gameFunctions();

    auto unit = fn.findUnitById(objectMap, &unitId.id);

    if (unit == nullptr)
        return false;

    auto modId = IdView{modifierId};

    hooks::addModifier(unit, &modId.id, true);

    return true;
}

bool ScenarioView::removeUnitModifier(const IdView& unitId, const std::string& modifierId)
{
    using namespace game;
    auto& fn = gameFunctions();
    auto& MidUnitApi = CMidUnitApi::get();

    auto unit = fn.findUnitById(objectMap, &unitId.id);

    if (unit == nullptr)
        return false;

    auto modId = IdView{modifierId};

    // hooks::removeModifierHooked(unit, 1, &modId.id);

    MidUnitApi.removeModifier(unit, &modId.id);

    return true;
}

bool ScenarioView::setTransform(const IdView& unitId, const std::string& unitIdTransform, bool saveXp)
{
    using namespace game;

    const auto& fn = gameFunctions();

    const auto& unitApi = CMidUnitApi::get();
    const auto& listApi = IdListApi::get();
    const auto& rttiApi = RttiApi::get();
    const auto& typeIdOperator = *rttiApi.typeIdOperator;
    const auto& typeInfoInequalityOperator = *rttiApi.typeInfoInequalityOperator;

    CMidUnit* attackUnit = fn.findUnitById(objectMap, &unitId.id);

    TUsUnitImpl* upgradeUnitImpl = nullptr;
    auto transUnit = IdView{unitIdTransform};

    if (transUnit == &game::emptyId)
    {
        auto playerIdByUnit = hooks::getPlayerIdByUnitId(objectMap, &unitId.id);
        auto playerId = hooks::getPlayer(objectMap, &playerIdByUnit);
        upgradeUnitImpl = const_cast<TUsUnitImpl*>(
        hooks::getUpgradeUnitImpl(objectMap, playerId, attackUnit));
    }
    else
    {
        upgradeUnitImpl = hooks::getUnitImpl(&transUnit.id);
    }

    //upgradeUnitImpl = hooks::getUnitImpl(transUnit);
    if (!upgradeUnitImpl)
        return false;

    auto unitImpl = hooks::getUnitImpl(attackUnit->unitImpl);
    if (typeInfoInequalityOperator(typeIdOperator(unitImpl), typeIdOperator(upgradeUnitImpl)))
        return false;

    IdList modifierIds{};
    listApi.constructor(&modifierIds);
    if (!unitApi.getModifiers(&modifierIds, attackUnit)
        || !unitApi.removeModifiers(&attackUnit->unitImpl)
        || !unitApi.replaceImpl(&attackUnit->unitImpl, upgradeUnitImpl)
        || !unitApi.addModifiers(&modifierIds, attackUnit, nullptr, true)) {
        listApi.destructor(&modifierIds);
        return false;
    }
    listApi.destructor(&modifierIds);

    // Reset XP first, because getHitPoints can return different values depending on current XP in
    // case of custom modifiers (has real examples in MNS)
    auto soldier = fn.castUnitImplToSoldier(attackUnit->unitImpl);
    attackUnit->currentXp = 0;
    attackUnit->currentHp = soldier->vftable->getHitPoints(soldier);
    
    auto stack = const_cast<CMidStack*>(hooks::getStackByUnitId(objectMap, &unitId.id));
    if (stack && stack->leaderId == unitId.id)
        stack->upgCount = 1;

    return true;
}

game::CBattleViewerInterf* findBattleViewerInterf()
{
    using namespace game;

    InterfManagerImplPtr managerPtr;
    CInterfManagerImplApi::get().get(&managerPtr);
    if (!managerPtr.data)
        return nullptr;

    const auto* batViewerVftable = BattleViewerInterfApi::vftable();
    auto& interfaces = managerPtr.data->data->interfaces;

    for (auto it = interfaces.begin(); it != interfaces.end(); ++it) {
        CInterface* interf = *it;
        if (!interf)
            continue;

        auto* viewer = reinterpret_cast<CBattleViewerInterf*>(interf);

        if (viewer->batViewer.vftable != batViewerVftable)
            continue;

        auto* dialog = CDragAndDropInterfApi::get().getDialog(
            reinterpret_cast<CDragAndDropInterf*>(interf));
        if (!dialog || !dialog->data)
            continue;

        if (strcmp(dialog->data->dialogName, "DLG_BATTLE_A") != 0)
            continue;

        return viewer;
    }

    return nullptr;
}

bool ScenarioView::addUnitXpWithUpgrade(const IdView& unitId, int exp)
{
    if (exp < 1)
        return false;

    using namespace game;

    static const auto& fn = gameFunctions();
    static const auto& unitApi = CMidUnitApi::get();
    static const auto& listApi = IdListApi::get();
    static const auto& rttiApi = RttiApi::get();
    static const auto& typeIdOperator = *rttiApi.typeIdOperator;
    static const auto& typeInfoInequalityOperator = *rttiApi.typeInfoInequalityOperator;

    CMidUnit* attackUnit = fn.findUnitById(objectMap, &unitId.id);
    if (!attackUnit)
        return false;

    auto soldier = fn.castUnitImplToSoldier(attackUnit->unitImpl);
    if (!soldier)
        return false;

    auto stack = const_cast<CMidStack*>(hooks::getStackByUnitId(objectMap, &unitId.id));
    const bool isLeader = stack && (stack->leaderId == unitId.id);

    if (exp > INT_MAX - attackUnit->currentXp)
        exp = INT_MAX;
    else
        exp += attackUnit->currentXp;

    const auto playerIdByUnit = hooks::getPlayerIdByUnitId(objectMap, &attackUnit->id);
    const auto player = hooks::getPlayer(objectMap, &playerIdByUnit);

    int currHp = attackUnit->currentHp;

    while (exp > 0) {
        const int xpNext = soldier->vftable->getXpNext(soldier);
        if (xpNext <= 0 || exp < xpNext)
            break;

        const auto upgradeUnitImpl = const_cast<TUsUnitImpl*>(
            hooks::getUpgradeUnitImpl(objectMap, player, attackUnit));
        if (!upgradeUnitImpl)
            break;

        const auto unitImpl = hooks::getUnitImpl(attackUnit->unitImpl);
        if (typeInfoInequalityOperator(typeIdOperator(unitImpl), typeIdOperator(upgradeUnitImpl)))
            break;

        IdList modifierIds{};
        listApi.constructor(&modifierIds);

        const bool success = unitApi.getModifiers(&modifierIds, attackUnit)
                             && unitApi.removeModifiers(&attackUnit->unitImpl)
                             && unitApi.replaceImpl(&attackUnit->unitImpl, upgradeUnitImpl)
                             && unitApi.addModifiers(&modifierIds, attackUnit, nullptr, true);

        listApi.destructor(&modifierIds);

        if (!success) {
            exp = xpNext - 1;
            break;
        }

        soldier = fn.castUnitImplToSoldier(attackUnit->unitImpl);
        if (!soldier) {
            exp = 0;
            break;
        }

        attackUnit->currentXp = 0;
        attackUnit->currentHp = soldier->vftable->getHitPoints(soldier);

        if (isLeader)
            stack->upgCount += 1;

        exp -= xpNext;
    }

    attackUnit->currentXp = exp;
    
    auto battleLogic = hooks::getBatLogic();

    if (battleLogic && battleLogic->battleMsgData) {
        attackUnit->currentHp = currHp;

        hooks::requestUnitVisualUpdate(attackUnit->id);

        //Update unit info
        hooks::heal(battleLogic->objectMap, battleLogic->battleMsgData, attackUnit, 0);
    }
    return true;
}

bool ScenarioView::giveSkillPoint(const IdView& stackId, int amout)
{
    using namespace game;

    if (!objectMap) {
        return false;
    }

    auto stack = hooks::getStack(objectMap, &stackId.id);
    if (!stack) {
        return false;
    }

    stack->upgCount += amout;

    return true;

}

std::optional<BagView> ScenarioView::getBag(const std::string& id) const
{
    return getBagById(IdView{id});
}

std::optional<BagView> ScenarioView::getBagById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    CMidBag* bag{static_cast<CMidBag*>(obj)};
    if (!bag) {
        return std::nullopt;
    }

    return BagView{bag, objectMap};
}

std::optional<BagView> ScenarioView::getBagByCoordinates(int x, int y) const
{
    auto bagId = getObjectId(x, y, game::IdType::Bag);
    if (!bagId) {
        return std::nullopt;
    }

    return getBagById(IdView{bagId});
}

std::optional<BagView> ScenarioView::getBagByPoint(const Point& p) const
{
    return getBagByCoordinates(p.x, p.y);
}

void ScenarioView::forEachBag(const std::function<void(const BagView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    auto runCallback = [this, &callback](const IMidScenarioObject* obj) {
        auto* bag{static_cast<const CMidBag*>(obj)};

        const BagView bagView{bag, objectMap};
        callback(bagView);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Bag, runCallback);
}

//Mage Tower
std::optional<MageTowerView> ScenarioView::getMageTower(const std::string& id) const
{
    return getMageTowerById(IdView{id});
}

std::optional<MageTowerView> ScenarioView::getMageTowerById(const IdView& id) const
{
    using namespace game;

    if (!objectMap) {
        return std::nullopt;
    }

    if (CMidgardIDApi::get().getType(&id.id) != IdType::Site) {
        return std::nullopt;
    }

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, &id.id);
    if (!obj) {
        return std::nullopt;
    }

    auto site = static_cast<const CMidSite*>(obj);
    if (site->siteCategory.id != SiteCategories::get().mageTower->id) {
        return std::nullopt;
    }

    return MageTowerView{static_cast<const CMidSiteMage*>(site), objectMap};
}

std::optional<MageTowerView> ScenarioView::getMageTowerByCoordinates(int x, int y) const
{
    auto mageId = getObjectId(x, y, game::IdType::Site);
    if (!mageId) {
        return std::nullopt;
    }

    return getMageTowerById(IdView{mageId});
}

std::optional<MageTowerView> ScenarioView::getMageTowerByPoint(const Point& p) const
{
    return getMageTowerByCoordinates(p.x, p.y);
}

void ScenarioView::forEachMageTower(const std::function<void(const MageTowerView&)>& callback) const
{
    if (!objectMap) {
        return;
    }

    using namespace game;

    const auto mageId{SiteCategories::get().mageTower->id};

    auto runCallback = [this, &callback, &mageId](const IMidScenarioObject* obj) {
        const auto* site{static_cast<const CMidSite*>(obj)};
        if (site->siteCategory.id != mageId) {
            return;
        }

        const MageTowerView view{static_cast<const CMidSiteMage*>(site), objectMap};
        callback(view);
    };

    hooks::forEachScenarioObject(objectMap, IdType::Site, runCallback);
}

bool ScenarioView::setVariableByName(const std::string& id, int value)
{
    using namespace game;

    CMidScenVariables* variables = const_cast<CMidScenVariables*>(
        hooks::getScenarioVariables(objectMap));

    if (!variables)
        return false;

    for (auto it = variables->variables.begin(); it != variables->variables.end(); ++it) {
        if (strcmp(it->second.name, id.c_str()) == 0) {
            it->second.value = value;
        }
    }

    return true;
}

bool ScenarioView::setVariableById(const int id, int value)
{
    using namespace game;

    CMidScenVariables* variables = const_cast<CMidScenVariables*>(hooks::getScenarioVariables(objectMap));

    if (!variables)
        return false;

    auto& scenVariablesApi = CMidScenVariablesApi::get();

    ScenarioVariableData* data = scenVariablesApi.findById(variables, id);
    if (data) {
        data->value = value;
        return true;
    }

    return false;
}

bool ScenarioView::createRod(IdView& ownerId, int x, int y)
{
    using namespace game;
    const auto& visitor = VisitorApi::get();
    const auto& posApi = CMqPointListApi::get();
    const auto& planApi = PlanDataApi::get();
    const auto& racesCat = RaceCategories::get();

    const auto pos = CMqPoint{x, y};
    const auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    if (visitor.createRod(&ownerId.id, &pos, obj, 1)) {
        auto& categories = TerrainCategories::get();

        CMidPlayer* player = static_cast<CMidPlayer*>(objectMap->vftable->findScenarioObjectById(objectMap, &ownerId.id));

        LRaceCategory* playerRace = &player->raceType->data->raceType;

        game::LTerrainCategory* terrainCategory = nullptr;
        switch (playerRace->id) {
        case RaceId::Human:
            terrainCategory = categories.human;
            break;
        case RaceId::Heretic:
            terrainCategory = categories.heretic;
            break;
        case RaceId::Undead:
            terrainCategory = categories.undead;
            break;
        case RaceId::Dwarf:
            terrainCategory = categories.dwarf;
            break;
        case RaceId::Elf:
            terrainCategory = categories.elf;
            break;
        default:
            return false;
        }


        CMqPointList positions{};
        posApi.ctor(&positions);
        posApi.add(&positions, &pos);

        PlanDataBuffer planData{};
        int zero{};
        char zeroChar{};
        planApi.ctor(&planData, &zeroChar, &zero, 0);

        visitor.changeMapTerrain(terrainCategory, &positions, planData.data, obj, 1);

        planApi.dtor(&planData);
        posApi.dtor(&positions);

        return true;
    }

    return false;
}

/*
const game::LTerrainCategory* getTerrainCategory(const game::LRaceCategory* raceCategory)
{
    using namespace game;

    const auto& terrains = TerrainCategories::get();
    switch (raceCategory->id) {
    case RaceId::Human:
        return terrains.human;
    case RaceId::Heretic:
        return terrains.heretic;
    case RaceId::Undead:
        return terrains.undead;
    case RaceId::Dwarf:
        return terrains.dwarf;
    case RaceId::Elf:
        return terrains.elf;
    default:
        return terrains.neutral;
    }
}
*/

bool ScenarioView::mapChangeTerrain(const int terrain, int x, int y)
{
    using namespace game;

    const auto& visitor = VisitorApi::get();
    const auto& posApi = CMqPointListApi::get();
    const auto& planApi = PlanDataApi::get();
    const auto& fn = gameFunctions();

    const auto obj = const_cast<IMidgardObjectMap*>(objectMap);
    const auto pos = CMqPoint{x, y};

    const CMidgardID* stackID = getObjectId(x, y, game::IdType::Stack);
    CMidStack* midStack = nullptr;
    if (stackID) {
        midStack = static_cast<CMidStack*>(objectMap->vftable->findScenarioObjectByIdForChange(obj, stackID));
        CMidUnit* leaderUnit = fn.findUnitById(objectMap, &midStack->leaderId);
        IUsSoldier* leaderSoldier = fn.castUnitImplToSoldier(leaderUnit->unitImpl);
        if (leaderSoldier->vftable->getWaterOnly(leaderSoldier))
            return false;
    }

    CMqPointList positions{};
    posApi.ctor(&positions);
    posApi.add(&positions, &pos);

    PlanDataBuffer planData{};
    int zero{};
    char zeroChar{};
    planApi.ctor(&planData, &zeroChar, &zero, 0);

    auto& categories = TerrainCategories::get();

    auto terrainId = static_cast<TerrainId>(terrain);
    game::LTerrainCategory* terrainCategory = nullptr;

    switch (terrainId) {
    case TerrainId::Human:
        terrainCategory = categories.human;
        break;
    case TerrainId::Dwarf:
        terrainCategory = categories.dwarf;
        break;
    case TerrainId::Heretic:
        terrainCategory = categories.heretic;
        break;
    case TerrainId::Undead:
        terrainCategory = categories.undead;
        break;
    case TerrainId::Neutral:
        terrainCategory = categories.neutral;
        break;
    case TerrainId::Elf:
        terrainCategory = categories.elf;
        break;
    default:
        planApi.dtor(&planData);
        posApi.dtor(&positions);
        return false;
    }

    visitor.changeMapTerrain(terrainCategory, &positions, planData.data, obj, 1);

    if (midStack) {
        midStack->facing = (midStack->facing + 1) % 8;
    }

    planApi.dtor(&planData);
    posApi.dtor(&positions);

    return true;
}

bool ScenarioView::moveStack(IdView& stackId, int x, int y)
{
    using namespace game;

    const auto& visitor = VisitorApi::get();

    const auto pos = CMqPoint{x, y};
    const auto obj = const_cast<IMidgardObjectMap*>(objectMap);

    auto stack = (CMidStack*)obj->vftable->findScenarioObjectByIdForChange(obj, &stackId.id);

    visitor.moveStack(&stackId.id, &pos, obj, 1);
    CMidStackApi::get().setPosition(stack, obj, &pos, false);




    //visitor.changeStackMoveAllowance(&stack->id, stack->movement, obj, 1);
    /*auto stack = hooks::getStack(objectMap, &stackId.id);
    
    visitor.moveStack(&stackId.id, &pos, obj, 1);

    auto stack = hooks::getStack(objectMap, &stackId.id);

    const auto& idSetApi = IdSetApi::get();

    auto midgard = CMidgardApi::get().instance();
    auto server = midgard->data->server;
    auto serverLogic = server->data->serverLogic;

    auto client = midgard->data->client;
    auto clientLogic = client->data->phase;*/

    //auto scenarioMap = CMidServerLogicApi::get().getObjectMap(serverLogic);

    //Pair<IdSetIterator, bool> tmp;
    //idSetApi.insert(&scenarioMap->changedObjects, &tmp, &stackId.id);

    return true;
}

} // namespace bindings
