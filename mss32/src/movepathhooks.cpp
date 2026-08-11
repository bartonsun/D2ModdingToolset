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
#include "fortification.h"
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
#include <algorithm>
#include <array>
#include <cmath>
#include <spdlog/spdlog.h>

#include "fortview.h"
#include "ruinview.h"
#include "scripts.h"
#include "siteview.h"
#include "stackview.h"
#include <gameutils.h>
#include <optional>
#include <sol/sol.hpp>
#include <usersettings.h>
#include <waitingmovepathflag.h>
#include <waitingmovepathhooks.h>
#include <waitingmovepathplanner.h>

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
    CMqPoint correctedLastReachablePoint{};
    bool pathLeadsToAction{};
    const game::CMidgardID* targetStackId = nullptr;
    const bool terrainOnlyPreview{isWaitingMovementPathPreview()};
    if (!a6) {
        positionPtr = lastReachablePoint;
    } else {
        positionPtr = lastReachablePoint;

        const bool targetTileVisible = !terrainOnlyPreview
                                       || isWaitingMovementPathPreviewTileVisible(objectMap,
                                                                                  pathEnd);
        if (targetTileVisible) {
            targetStackId = fn.getBlockingPathNearbyStackId(objectMap, plan, stack,
                                                            lastReachablePoint, pathEnd, 0);
        }

        if (terrainOnlyPreview && targetStackId) {
            const auto* targetStack = getStack(objectMap, targetStackId);
            if (!targetStack || targetStack->invisible
                || !isWaitingMovementPathPreviewTileVisible(objectMap, &targetStack->position)) {
                targetStackId = nullptr;
            }
        }

        pathLeadsToAction = targetStackId != nullptr;

        if (!terrainOnlyPreview && !pathLeadsToAction) {
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

    const bool secondSegmentPreview = isWaitingMovementPathSecondSegmentPreview();
    const auto stackPosition = fn.getStackPositionById(objectMap, stackId);
    if (terrainOnlyPreview && !secondSegmentPreview) {
        clearWaitingMovementPathBattleContext();
    }

    if (terrainOnlyPreview && !secondSegmentPreview && stack->insideId == emptyId && plan) {
        const IdType fortType = IdType::Fortification;
        const auto* fortId = CMidgardPlanApi::get().getObjectId(plan, pathEnd, &fortType);
        const auto* fort = fortId ? getFort(objectMap, fortId) : nullptr;
        CMqPoint entrance{};
        if (fort && canEnterWaitingMovementPathFort(objectMap, stack, fort)
            && isWaitingMovementPathPreviewTileVisible(objectMap, pathEnd)
            && fn.getFortOrRuinEntrance(objectMap, plan, stack, pathEnd, &entrance)
            && isWaitingMovementPathPreviewTileVisible(objectMap, &entrance)) {
            auto* tail = pathInfo.head->prev;
            if (tail != pathInfo.head && tail->data.position == entrance) {
                tail->data.moveCostTotal = tail->prev == pathInfo.head
                                               ? 0
                                               : tail->prev->data.moveCostTotal;
            }
        }
    }

    if (terrainOnlyPreview && !secondSegmentPreview
        && CMidgardIDApi::get().getType(&stack->insideId) == IdType::Fortification) {
        const auto* fort = getFort(objectMap, &stack->insideId);
        if (canExitWaitingMovementPathFort(stack, fort)) {
            const auto entrance = getObjectEntrance(fort->mapElement.position,
                                                    fort->mapElement.sizeX, fort->mapElement.sizeY);
            PathInfoListNode* firstExitNode{};
            int entranceCost{};
            if (stackPosition == entrance) {
                for (auto* node = pathInfo.head->next; node != pathInfo.head; node = node->next) {
                    if (node->data.position == entrance) {
                        entranceCost = node->data.moveCostTotal;
                        continue;
                    }

                    firstExitNode = node;
                    break;
                }
            }

            if (firstExitNode && std::abs(firstExitNode->data.position.x - entrance.x) <= 1
                && std::abs(firstExitNode->data.position.y - entrance.y) <= 1) {
                const int freeExitCost = std::max(0,
                                                  firstExitNode->data.moveCostTotal - entranceCost);
                for (auto* node = firstExitNode; node != pathInfo.head; node = node->next) {
                    node->data.moveCostTotal = std::max(0, node->data.moveCostTotal - freeExitCost);
                }
            }
        }
    }

    if (terrainOnlyPreview && !secondSegmentPreview) {
        correctedLastReachablePoint = stackPosition;
        for (auto* node = pathInfo.head->next; node != pathInfo.head; node = node->next) {
            if (node->data.moveCostTotal <= maxMovement) {
                correctedLastReachablePoint = node->data.position;
            }
        }
        positionPtr = &correctedLastReachablePoint;

        if (a6 && plan) {
            targetStackId = nullptr;
            if (isWaitingMovementPathPreviewTileVisible(objectMap, pathEnd)) {
                targetStackId = fn.getBlockingPathNearbyStackId(objectMap, plan, stack, positionPtr,
                                                                pathEnd, 0);
            }

            if (targetStackId) {
                const auto* targetStack = getStack(objectMap, targetStackId);
                if (!targetStack || targetStack->invisible
                    || !isWaitingMovementPathPreviewTileVisible(objectMap,
                                                                &targetStack->position)) {
                    targetStackId = nullptr;
                }
            }
            pathLeadsToAction = targetStackId != nullptr;
        }
    }

    if (terrainOnlyPreview) {
        const int budget = secondSegmentPreview ? getWaitingMovementPathSecondSegmentBudget()
                                                : maxMovement;
        for (auto node = pathInfo.head->next; node != pathInfo.head; node = node->next) {
            if (secondSegmentPreview) {
                int cost{};
                if (!getWaitingMovementPathSecondSegmentCost(&node->data.position, &cost)) {
                    continue;
                }

                node->data.moveCostTotal = cost;
            }

            const int cost = node->data.moveCostTotal;
            node->data.turnsToReach = cost <= budget || maxMovement <= 0
                                          ? 0
                                          : 1 + (cost - budget - 1) / maxMovement;
        }
    }

    if (terrainOnlyPreview) {
        spdlog::debug("Waiting movement path preview render prepared: path={}, display={}",
                      path->length, pathInfo.length);
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

    const auto& moveCostColor{userSettings().movementDisplay.textColor};
    const auto& moveCostOutline{userSettings().movementDisplay.outlineColor};

    bool firstNode{true};

    bool pathAllowed{};
    bool waterOnlyToLand{};

    bool v61 = *positionPtr != stackPosition;

    int turnNumber{};
    bool manyTurnsToTravel{};

    std::uint32_t index{};
    const bool altPressed = GetAsyncKeyState(VK_MENU) & 0x8000;

    CIsoLayer customLayer = *isoLayers().symMovePath;
    customLayer.value *= 3;
    CIsoLayer secondSegmentLayer = *isoLayers().symMovePath;
    secondSegmentLayer.value *= 4;

    const bool waitingPreviewActive = isWaitingMovementPathPreviewActive();
    MapGraphicsApi::get().hideLayerImages(isoLayers().symMovePath);
    if (secondSegmentPreview) {
        MapGraphicsApi::get().hideLayerImages(&secondSegmentLayer);
    } else if (terrainOnlyPreview) {
        MapGraphicsApi::get().hideLayerImages(&customLayer);
    } else if (!waitingPreviewActive) {
        MapGraphicsApi::get().hideLayerImages(&customLayer);
        MapGraphicsApi::get().hideLayerImages(&secondSegmentLayer);
    }

    const CIsoLayer* drawLayer = secondSegmentPreview ? &secondSegmentLayer
                                 : terrainOnlyPreview || (altPressed && !waitingPreviewActive)
                                     ? &customLayer
                                     : isoLayers().symMovePath;

    std::uint32_t imagesShown{};

    for (auto node = pathInfo.head->next; node != pathInfo.head;
         node = node->next, firstNode = false, ++index) {
        const auto& currentPosition = node->data.position;

        if (!terrainOnlyPreview
            && !fn.stackCanMoveToPosition(objectMap, &currentPosition, stack, plan)) {
            continue;
        }

        pathAllowed = !waterOnlyToLand;

        if (waterOnly) {
            if (terrainOnlyPreview
                && !isWaitingMovementPathPreviewAreaVisible(objectMap, &currentPosition)) {
                pathAllowed = false;
                waterOnlyToLand = true;
            } else if (!fn.isWaterTileSurroundedByWater(&currentPosition, objectMap)) {
                pathAllowed = false;
                waterOnlyToLand = true;
            }
        }

        const bool endOfPath{currentPosition == *positionPtr};
        const int secondSegmentBudget = secondSegmentPreview
                                            ? getWaitingMovementPathSecondSegmentBudget()
                                            : 0;
        const int waitingBudget = secondSegmentPreview ? secondSegmentBudget : maxMovement;
        const int nodeCost = node->data.moveCostTotal;
        const bool waitingNodeReachable = !terrainOnlyPreview || nodeCost <= waitingBudget;
        const bool waitingActionReachable = terrainOnlyPreview && endOfPath && pathLeadsToAction
                                            && nodeCost < waitingBudget;

        const char* imageName = "MOVENORMAL";
        bool useGrayWaitingFlag{};
        if (terrainOnlyPreview) {
            if (!waitingNodeReachable
                || (endOfPath && pathLeadsToAction && !waitingActionReachable)) {
                imageName = "MOVEACTION";
            } else if (waitingActionReachable) {
                imageName = noble ? "MOVENEGO" : "MOVEBATTLE";
            } else {
                useGrayWaitingFlag = true;
            }
        } else if (!v61) {
            imageName = pathLeadsToAction ? "MOVEOUT" : "MOVEACTION";
        }

        if (endOfPath) {
            v61 = false;
            if (!terrainOnlyPreview && pathLeadsToAction) {
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
            useGrayWaitingFlag = false;
        }

        game::IMqImage2* flagImage{};
        if (useGrayWaitingFlag) {
            flagImage = createWaitingMovementPathGrayFlag();
        }
        if (!flagImage) {
            flagImage = imagesApi.getImage(images->isoCmon, imageName, 0, true, images->log);
        }
        if (!flagImage) {
            continue;
        }

        const auto imagesCount{flagImage->vftable->getImagesCount(flagImage)};
        if (imagesCount > 0) {
            flagImage->vftable->setImageIndex(flagImage, index % imagesCount);
        }

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

            const int spent = node->data.moveCostTotal;
            std::string moveText = secondSegmentPreview
                                       ? fmt::format("{}", std::max(0, secondSegmentBudget - spent))
                                       : fmt::format("{}", spent);

            const bool isActionFlag = terrainOnlyPreview ? waitingActionReachable
                                                         : endOfPath && pathLeadsToAction;

            if (isActionFlag) {
                int currentMovement = static_cast<int>(stack->movement);
                if (terrainOnlyPreview) {
                    currentMovement = maxMovement;
                }
                if (secondSegmentPreview) {
                    currentMovement = getWaitingMovementPathSecondSegmentBudget();
                }
                const int remaining = std::max(0, currentMovement - spent);

                //
                // Default formula
                //

                int movementAfterAction = std::max(0, remaining - (maxMovement + 1) / 2);

                if (terrainOnlyPreview && !secondSegmentPreview && targetStackId) {
                    setWaitingMovementPathBattleContext(&currentPosition, targetStackId,
                                                        movementAfterAction, maxMovement);
                }

                if (terrainOnlyPreview || userSettings().movementDisplay.showMovementAfterAction) {
                    //
                    // Lazy load
                    //

                    if (!movementActionPenaltyLua) {

                        const auto path{scriptsFolder() / "movement.lua"};

                        movementActionPenaltyLua = getScriptFunction(path, "movementAfterAction",
                                                                     env, false, true);
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

                            movementContext["currentMovement"] = currentMovement;

                            movementContext["spentMovement"] = static_cast<int>(spent);

                            movementContext["remainingMovement"] = static_cast<int>(remaining);

                            movementContext["afterActionMovement"] = static_cast<int>(
                                movementAfterAction);

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

                    if (terrainOnlyPreview && !secondSegmentPreview && targetStackId) {
                        setWaitingMovementPathBattleContext(&currentPosition, targetStackId,
                                                            movementAfterAction, maxMovement);
                    }

                    moveText = secondSegmentPreview
                                   ? fmt::format("{} ({})", remaining, movementAfterAction)
                                   : fmt::format("{} ({})", spent, movementAfterAction);
                }

                if (terrainOnlyPreview) {
                    spdlog::debug(
                        "Waiting movement path preview battle budget: segment={}, current={}, "
                        "spent={}, penalty={}, after={}",
                        secondSegmentPreview ? 2 : 1, currentMovement, spent, (maxMovement + 1) / 2,
                        movementAfterAction);
                }
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
        ++imagesShown;
    }

    if (terrainOnlyPreview) {
        spdlog::debug("Waiting movement path preview rendered: images={}", imagesShown);
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

    const bool terrainOnlyPreview{isWaitingMovementPathPreview()};
    if (terrainOnlyPreview && !isWaitingMovementPathPreviewTileVisible(objectMap, mapPosition)) {
        return movementForbidden;
    }

    const bool pred1 = !a6 || !((1 << (x & 7)) & a6[18 * y + (x >> 3)]);
    if (!pred1) {
        const IdType bagType = IdType::Bag;
        const IdType stackType = IdType::Stack;
        static const std::array<IdType, 7> blockingObjectTypes{{
            IdType::Fortification,
            IdType::Landmark,
            IdType::Site,
            IdType::Ruin,
            IdType::Tomb,
            IdType::Rod,
            IdType::Crystal,
        }};
        const auto& planApi = CMidgardPlanApi::get();
        const auto* stackAtPosition = planApi.getObjectId(plan, mapPosition, &stackType);
        const auto* blockingStack = stackAtPosition ? getStack(objectMap, stackAtPosition)
                                                    : nullptr;
        const auto* previewStack = getStack(objectMap, stackId);
        const bool sameStack = stackAtPosition && isIdsEqualOrBothNull(stackAtPosition, stackId);
        const bool hiddenForeignStack = blockingStack && previewStack && blockingStack->invisible
                                        && blockingStack->ownerId != previewStack->ownerId;
        const bool stackPassable = !stackAtPosition || sameStack || hiddenForeignStack;
        const bool bypassReason = planApi.getObjectId(plan, mapPosition, &bagType) || sameStack
                                  || hiddenForeignStack;
        const bool passablePreviewTile = terrainOnlyPreview && stackPassable && bypassReason
                                         && !planApi.isPositionContainsObjects(
                                             plan, mapPosition, blockingObjectTypes.data(),
                                             std::size(blockingObjectTypes));
        if (!passablePreviewTile) {
            return movementForbidden;
        }
    }

    bool road{};

    if (terrainOnlyPreview) {
        const IdType roadType = IdType::Road;
        road = CMidgardPlanApi::get().getObjectId(plan, mapPosition, &roadType) != nullptr;
    } else {
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
        road = planApi.getObjectId(plan, mapPosition, &roadType) != nullptr;
    }

    LGroundCategory ground{};
    if (!CMidgardMapApi::get().getGround(midgardMap, &ground, mapPosition, objectMap)) {
        return movementForbidden;
    }

    const auto& groundTypes = GroundCategories::get();

    if (ground.id == groundTypes.water->id) {
        const auto& water = gameSettings().movementCost.water;

        if (!waterOnly) {
            if (leaderAlive) {
                return waterBonus ? water.withBonus : water.dflt;
            }

            return water.deadLeader;
        }

        // Check deep waters
        if (terrainOnlyPreview
            && !isWaitingMovementPathPreviewAreaVisible(objectMap, mapPosition)) {
            return movementForbidden;
        }

        if (gameFunctions().isWaterTileSurroundedByWater(mapPosition, objectMap)) {
            return water.waterOnly;
        }
    } else if (ground.id == groundTypes.forest->id) {
        const auto& forest = gameSettings().movementCost.forest;

        if (!waterOnly) {
            if (leaderAlive) {
                return forestBonus ? forest.withBonus : forest.dflt;
            }

            return forest.deadLeader;
        }
    } else if (ground.id == groundTypes.plain->id) {
        const auto& plain = gameSettings().movementCost.plain;

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
