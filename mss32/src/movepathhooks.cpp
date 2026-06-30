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

#include "movepathhooks.h"
#include "dynamiccast.h"
#include "game.h"
#include "gameimages.h"
#include "gamesettings.h"
#include "groundcat.h"
#include "image2text.h"
#include "isolayers.h"
#include "mapgraphics.h"
#include "mempool.h"
#include "midgard.h"
#include "midgardmap.h"
#include "midgardobjectmap.h"
#include "midgardplan.h"
#include "midstack.h"
#include "midunit.h"
#include "multilayerimg.h"
#include "pathinfolist.h"
#include "settings.h"
#include "ussoldier.h"
#include "usstackleader.h"
#include "utils.h"
#include <array>
#include <cmath>
#include <spdlog/spdlog.h>

#include "scripts.h"
#include <optional>
#include <sol/sol.hpp>
#include "stackview.h"
#include "fortview.h"
#include "ruinview.h"
#include "siteview.h"
#include <gameutils.h>

namespace hooks {

static std::optional<sol::environment> env;
static std::optional<sol::function> movementActionPenaltyLua;
static bool movementActionPenaltyLuaErrorShown = false;

static bool isIdsEqualOrBothNull(const game::CMidgardID* id1, const game::CMidgardID* id2)
{
    if (!id1 && !id2) {
        // Both null, treat as equal
        return true;
    }

    if (id1 && id2) {
        return *id1 == *id2;
    }

    return false;
};

static void fillMovementTargetContext(sol::table& movementContext,
                                      const game::IMidgardObjectMap* objectMap,
                                      const game::CMidgardPlan* plan,
                                      const game::CMqPoint* pathEnd,
                                      const game::CMidgardID* targetStackId)
{
    using namespace game;

    //
    // Stack has the highest priority.
    // The game already resolved which stack is the action target.
    //

    if (targetStackId) {

        if (hooks::getStack(objectMap, targetStackId)) {

            movementContext["targetId"] = bindings::IdView(*targetStackId);
            return;
        }
    }

    //
    // No stack target.
    // Check the destination tile for other interactive objects.
    // Only the target id is exposed to Lua. The corresponding View
    // can be obtained later using the existing API.
    //

    const auto& planApi = CMidgardPlanApi::get();

    //
    // Fortification (city / capital)
    //

    {
        const IdType type = IdType::Fortification;

        if (const auto* id = planApi.getObjectId(plan, pathEnd, &type)) {

            if (hooks::getFort(objectMap, id)) {

                movementContext["targetId"] = bindings::IdView(*id);
                return;
            }
        }
    }

    //
    // Ruin
    //

    {
        const IdType type = IdType::Ruin;

        if (const auto* id = planApi.getObjectId(plan, pathEnd, &type)) {

            if (hooks::getRuin(objectMap, id)) {

                movementContext["targetId"] = bindings::IdView(*id);
                return;
            }
        }
    }

    //
    // Nothing found.
    // targetId is left unset.
    //
}

void __stdcall showMovementPathHooked(const game::IMidgardObjectMap* objectMap,
                                      const game::CMidgardID* stackId,
                                      game::List<game::CMqPoint>* path,
                                      const game::CMqPoint* lastReachablePoint,
                                      const game::CMqPoint* pathEnd,
                                      bool a6)
{
    using namespace game;

    const auto& fn = gameFunctions();

    auto plan = fn.getMidgardPlan(objectMap);

    const auto& dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto stackObj = objectMap->vftable->findScenarioObjectById(objectMap, stackId);
    auto stack = static_cast<const CMidStack*>(
        dynamicCast(stackObj, 0, rtti.IMidScenarioObjectType, rtti.CMidStackType, 0));

    auto leaderObj = objectMap->vftable->findScenarioObjectById(objectMap, &stack->leaderId);
    auto leader = static_cast<const CMidUnit*>(leaderObj);
    auto unitImpl = leader->unitImpl;

    auto stackLeader = fn.castUnitImplToStackLeader(unitImpl);

    int maxMovement = 0;

    if (stackLeader)
        maxMovement = stackLeader->vftable->getMovement(stackLeader);


    const bool noble = fn.castUnitImplToNoble(unitImpl) != nullptr;

    auto soldier = fn.castUnitImplToSoldier(unitImpl);
    const bool waterOnly = soldier->vftable->getWaterOnly(soldier);

    const CMqPoint* positionPtr{};
    bool pathLeadsToAction{};
    const game::CMidgardID* targetStackId = nullptr;
    if (!a6) {
        positionPtr = lastReachablePoint;
    } else {
        positionPtr = lastReachablePoint;

        targetStackId = fn.getBlockingPathNearbyStackId(objectMap, plan, stack, lastReachablePoint,
                                                        pathEnd, 0);

        pathLeadsToAction = targetStackId != nullptr;

        if (!pathLeadsToAction) {
            CMqPoint entrance{};
            if (fn.getFortOrRuinEntrance(objectMap, plan, stack, pathEnd, &entrance)
                && std::abs(lastReachablePoint->x - entrance.x) <= 1
                && std::abs(lastReachablePoint->y - entrance.y) <= 1) {
                pathLeadsToAction = true;
            }
        }
    }

    const auto& pathApi = PathInfoListApi::get();

    PathInfoList pathInfo;
    pathApi.constructor(&pathInfo);

    {
        CMqPoint point{};
        point.x = positionPtr->x;
        point.y = positionPtr->y;
        pathApi.populateFromPath(objectMap, stack, path, &point, waterOnly, &pathInfo);
    }

    const auto& imagesApi = GameImagesApi::get();

    GameImagesPtr imagesPtr;
    imagesApi.getGameImages(&imagesPtr);
    auto images = *imagesPtr.data;

    const auto& memAlloc = Memory::get().allocate;

    auto gameSettings = *CMidgardApi::get().instance()->data->settings;
    const bool displayPathTurn{gameSettings->displayPathTurn};
    CMidgardID turnStringId{};
    CMidgardIDApi::get().fromString(&turnStringId, "X005TA0935");
    const char* turnString{fn.getInterfaceText(&turnStringId)};

    const auto& moveCostColor{userSettings().movementCost.textColor};
    const auto& moveCostOutline{userSettings().movementCost.outlineColor};

    bool firstNode{true};

    bool pathAllowed{};
    bool waterOnlyToLand{};

    const auto stackPosition = fn.getStackPositionById(objectMap, stackId);
    bool v61 = *positionPtr != stackPosition;

    int turnNumber{};
    bool manyTurnsToTravel{};

    std::uint32_t index{};
    const bool altPressed = GetAsyncKeyState(VK_MENU) & 0x8000;

    CIsoLayer customLayer = *isoLayers().symMovePath;
    customLayer.value *= 3;

   // Always clear previous path images, even if alt is pressed, because we need to hide previous
    // path images from the default layer.
    MapGraphicsApi::get().hideLayerImages(isoLayers().symMovePath);
    MapGraphicsApi::get().hideLayerImages(&customLayer);

    const CIsoLayer* drawLayer = altPressed ? &customLayer : isoLayers().symMovePath;

    for (auto node = pathInfo.head->next; node != pathInfo.head;
         node = node->next, firstNode = false, ++index) {
        const auto& currentPosition = node->data.position;

        if (!fn.stackCanMoveToPosition(objectMap, &currentPosition, stack, plan)) {
            continue;
        }

        pathAllowed = !waterOnlyToLand;

        if (waterOnly && !fn.isWaterTileSurroundedByWater(&currentPosition, objectMap)) {
            pathAllowed = false;
            waterOnlyToLand = true;
        }

        // Blue flag
        const char* imageName = "MOVENORMAL";
        if (!v61) {
            // Red flag
            imageName = "MOVEOUT";
            if (!pathLeadsToAction) {
                // White flag
                imageName = "MOVEACTION";
            }
        }

        const bool endOfPath{currentPosition == *positionPtr};
        if (endOfPath) {
            v61 = false;
            if (pathLeadsToAction) {
                // Red flag with a scroll, noble actions
                imageName = "MOVENEGO";

                if (!noble) {
                    // Red flag with a sword, battle
                    imageName = "MOVEBATTLE";
                }
            }
        }

        if (!pathAllowed) {
            // Crossed out white flag, when path of water only stack leads to the land
            imageName = "MOVEINCMP";
        }

        auto flagImage = imagesApi.getImage(images->isoCmon, imageName, 0, true, images->log);
        if (!flagImage) {
            continue;
        }

        const auto imagesCount{flagImage->vftable->getImagesCount(flagImage)};
        flagImage->vftable->setImageIndex(flagImage, index % imagesCount);

        CImage2Text* turnNumberImage{};

        if (displayPathTurn && !firstNode) {
            bool drawTurnNumber{};
            turnNumber = 0;

            const auto prev = node->prev;
            const auto prevTurnsToReach{prev->data.turnsToReach};
            const auto currTurnsToReach{node->data.turnsToReach};
            const bool differ{prevTurnsToReach != currTurnsToReach};

            // Check previous node, draw turn number only if number of turns to reach is different
            // and previous one is not 0
            if (differ) {
                manyTurnsToTravel = true;

                if (prevTurnsToReach != 0) {
                    drawTurnNumber = true;
                    turnNumber = prevTurnsToReach;
                }
            }

            const bool lastNode{node->next == pathInfo.head};
            if (lastNode && manyTurnsToTravel) {
                // Always draw turn number on the last path node,
                // but only when path is longer than single turn
                drawTurnNumber = true;

                // Special case: end of the path is the first node of a new travel turn
                // (previous node has different turns to reach), we draw _previous_ day turn number.
                // Exception: do not draw 0 as turn number
                if (!differ || prev->data.turnsToReach == 0) {
                    turnNumber = currTurnsToReach;
                } else {
                    turnNumber = prevTurnsToReach;
                }
            }

            if (drawTurnNumber) {
                turnNumberImage = static_cast<CImage2Text*>(memAlloc(sizeof(CImage2Text)));
                CImage2TextApi::get().constructor(turnNumberImage, 32, 64);

                std::string text{turnString};
                replace(text, "%TURN%", fmt::format("{:d}", turnNumber));
                CImage2TextApi::get().setText(turnNumberImage, text.c_str());
            }
        }

        CImage2Text* moveCostImage{};

        if (pathAllowed && !turnNumberImage) {
            moveCostImage = static_cast<CImage2Text*>(memAlloc(sizeof(CImage2Text)));
            CImage2TextApi::get().constructor(moveCostImage, 64, 64);

            std::string moveText = fmt::format("{}", node->data.moveCostTotal);

            const bool isActionFlag = endOfPath && pathLeadsToAction;

            if (isActionFlag && userSettings().movementCost.showMovementAfterAction) {

                const int spent = node->data.moveCostTotal;

                const int remaining = std::max(0, static_cast<int>(stack->movement) - spent);

                //
                // Default formula
                //

                int movementAfterAction = std::max(0, remaining - (maxMovement + 1) / 2);

                //
                // Lazy load
                //

                if (!movementActionPenaltyLua) {

                    const auto path{scriptsFolder() / "movement.lua"};

                    movementActionPenaltyLua = getScriptFunction(path, "movementAfterAction", env,
                                                                 false, true);
                }

                //
                // Lua override
                //

                if (movementActionPenaltyLua) {

                    try {

                        sol::state_view lua(movementActionPenaltyLua->lua_state());

                        sol::table movementContext = lua.create_table();

                        movementContext["stack"] = bindings::StackView(stack, objectMap);

                        movementContext["maxMovement"] = maxMovement;

                        movementContext["currentMovement"] = static_cast<int>(stack->movement);

                        movementContext["spentMovement"] = static_cast<int>(spent);

                        movementContext["remainingMovement"] = static_cast<int> (remaining);

                        movementContext["afterActionMovement"] = static_cast<int>(movementAfterAction);

                        fillMovementTargetContext(movementContext, objectMap, plan, pathEnd,
                                                  targetStackId);

                        sol::object result = (*movementActionPenaltyLua)(movementContext);

                        if (result.is<std::int32_t>()) {
                            movementAfterAction = result.as<std::int32_t>();
                        }

                    } catch (const std::exception& e) {

                        // Force reload on next call.
                        movementActionPenaltyLua.reset();
                        env.reset();

                        spdlog::error("[MOVEMENT] movement.lua: {}", e.what());

                        if (!movementActionPenaltyLuaErrorShown) {

                            movementActionPenaltyLuaErrorShown = true;

                            const auto path{scriptsFolder() / "movement.lua"};

                            showErrorMessageBox(fmt::format("Failed to run "
                                                            "'{:s}' script.\n"
                                                            "Reason: '{:s}'",
                                                            path.string(), e.what()));
                        }
                    }
                }

                moveText = fmt::format("{} ({})", spent, movementAfterAction);
            }

            const auto moveCostString = fmt::format(
                "\\fmedium;\\hC;\\vT;\\c{:03d};{:03d};{:03d};\\o{:03d};{:03d};{:03d};{}",
                (int)moveCostColor.r, (int)moveCostColor.g, (int)moveCostColor.b,
                (int)moveCostOutline.r, (int)moveCostOutline.g, (int)moveCostOutline.b, moveText);

            CImage2TextApi::get().setText(moveCostImage, moveCostString.c_str());
        }

        auto multilayerImg = static_cast<CMultiLayerImg*>(memAlloc(sizeof(CMultiLayerImg)));
        CMultiLayerImgApi::get().constructor(multilayerImg);

        CMultiLayerImgApi::get().addImage(multilayerImg, flagImage, -999, -999);

        if (turnNumberImage) {
            CMultiLayerImgApi::get().addImage(multilayerImg, turnNumberImage, -999, -999);
        }

        if (moveCostImage) {
            CMultiLayerImgApi::get().addImage(multilayerImg, moveCostImage, -999, -999);
        }

        CMqPoint pos;
        pos.x = currentPosition.x;
        pos.y = currentPosition.y;

        MapGraphicsApi::get().showImageOnMap(&pos, drawLayer, multilayerImg, 0, 0);
    }

    imagesApi.createOrFreeGameImages(&imagesPtr, nullptr);

    pathApi.freeNodes(&pathInfo);
    pathApi.freeNode(&pathInfo, pathInfo.head);
}

int __stdcall computeMovementCostHooked(const game::CMqPoint* mapPosition,
                                        const game::IMidgardObjectMap* objectMap,
                                        const game::CMidgardMap* midgardMap,
                                        const game::CMidgardPlan* plan,
                                        const game::CMidgardID* stackId,
                                        const char* a6,
                                        const char* a7,
                                        bool leaderAlive,
                                        bool plainsBonus,
                                        bool forestBonus,
                                        bool waterBonus,
                                        bool waterOnly,
                                        bool forbidWaterOnlyOnLand)
{
    using namespace game;

    constexpr int movementForbidden = 0;

    const int x = mapPosition->x;
    const int y = mapPosition->y;

    if (x < 0 || x >= midgardMap->mapSize || y < 0 || y >= midgardMap->mapSize) {
        // Outside of map
        return movementForbidden;
    }

    const bool pred1 = !a6 || !((1 << (x & 7)) & a6[18 * y + (x >> 3)]);
    if (!pred1) {
        return movementForbidden;
    }

    // clang-format off
    static const std::array<IdType, 6> interactiveObjectTypes{{
        IdType::Fortification,
        IdType::Landmark,
        IdType::Site,
        IdType::Ruin,
        IdType::Rod,
        IdType::Crystal
    }};
    // clang-format on

    const auto& planApi = CMidgardPlanApi::get();

    if (planApi.isPositionContainsObjects(plan, mapPosition, interactiveObjectTypes.data(),
                                          std::size(interactiveObjectTypes))) {
        // Interactive object is in the way
        return movementForbidden;
    }

    const IdType bagType = IdType::Bag;
    const CMidgardID* bagId = planApi.getObjectId(plan, mapPosition, &bagType);

    const bool pred2 = a7 && ((1 << (x & 7)) & a7[18 * y + (x >> 3)]);

    if (!(!bagId || pred2)) {
        return movementForbidden;
    }

    const IdType stackType = IdType::Stack;
    const CMidgardID* stackAtPosition = planApi.getObjectId(plan, mapPosition, &stackType);

    if (!(!stackAtPosition || isIdsEqualOrBothNull(stackAtPosition, stackId) || pred2)) {
        return movementForbidden;
    }

    const IdType roadType = IdType::Road;
    const bool road = planApi.getObjectId(plan, mapPosition, &roadType) != nullptr;

    LGroundCategory ground{};
    if (!CMidgardMapApi::get().getGround(midgardMap, &ground, mapPosition, objectMap)) {
        return movementForbidden;
    }

    const auto& groundTypes = GroundCategories::get();

    if (ground.id == groundTypes.water->id) {
        const auto& water = userSettings().movementCost.water;

        if (!waterOnly) {
            if (leaderAlive) {
                return waterBonus ? water.withBonus : water.dflt;
            }

            return water.deadLeader;
        }

        // Check deep waters
        if (gameFunctions().isWaterTileSurroundedByWater(mapPosition, objectMap)) {
            return water.waterOnly;
        }
    } else if (ground.id == groundTypes.forest->id) {
        const auto& forest = userSettings().movementCost.forest;

        if (!waterOnly) {
            if (leaderAlive) {
                return forestBonus ? forest.withBonus : forest.dflt;
            }

            return forest.deadLeader;
        }
    } else if (ground.id == groundTypes.plain->id) {
        const auto& plain = userSettings().movementCost.plain;

        if (!waterOnly) {
            if (!leaderAlive) {
                return plain.deadLeader;
            }

            if (!plainsBonus && road) {
                return plain.onRoad;
            }

            return plain.dflt;
        }
    } else {
        // Mountain ground type
        return movementForbidden;
    }

    // This is the case when water-only stack tries to move on shore or land.
    // We don't care about ground type here.
    // Forbid movement so move points will not be wasted in attempt to perform illegal move
    if (forbidWaterOnlyOnLand) {
        return movementForbidden;
    }

    // Assumption:
    // Unreachable big value used by the game
    // to compute and show movement path on land for water-only stacks.
    // This logic is only to properly show movement path on land
    // while marking it as forbidden
    constexpr int moveCostWaterOnlyOnLand = 1000;
    return moveCostWaterOnlyOnLand;
}

} // namespace hooks
