/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Stanislav Egorov.
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

#include "battleviewerinterfhooks.h"
#include "attack.h"
#include "batimagesloader.h"
#include "battleviewerinterf.h"
#include "batviewer2dengine.h"
#include "game.h"
#include "midunitgroup.h"
#include "unitinfolist.h"
#include "unitpositionlist.h"

namespace hooks {

void markAttackTarget(game::CBattleViewerInterf* viewer,
                      const game::CMidgardID* targetId,
                      bool targetIsAttacker,
                      int targetPosition)
{
    using namespace game;

    const auto& viewerApi = BattleViewerInterfApi::get();
    const auto& rectApi = MqRectApi::get();
    const auto& loaderApi = BatImagesLoaderApi::get();
    const auto& engineApi = BatViewer2DEngineApi::get();

    bool isUnitBig = false;
    bool isUnitRetreating = false;
    if (*targetId != emptyId) {
        isUnitBig = viewerApi.isUnitBig(&viewer->data->unknownUnitData, targetId);
        isUnitRetreating = viewerApi.isUnitRetreating(&viewer->data->unknownUnitData, targetId);
    }

    CMqRect unitRect{};
    bool isUnitOnTheLeft = viewerApi.isUnitOnTheLeft(viewer, targetIsAttacker);
    viewerApi.getUnitRect(&unitRect,
                          isUnitOnTheLeft ? &viewer->data->groupAreas2 : &viewer->data->groupAreas1,
                          targetPosition, isUnitBig, isUnitRetreating);

    IMqImage2* image = loaderApi.loadMarkImage(viewer->data->imagesLoader, isUnitBig);
    if (image == nullptr)
        return;

    CMqPoint imageSize{};
    image->vftable->getSize(image, &imageSize);

    CMqPoint unitCenter{};
    rectApi.getCenter(&unitRect, &unitCenter);

    CMqPoint imagePosition{unitCenter.x - imageSize.x / 2, unitRect.p2.y - imageSize.y / 2};
    engineApi.drawTargetMark(viewer->data->batViewer2dEngine, image, &imagePosition,
                             isUnitOnTheLeft, targetPosition);
}

void markAllAttackTargets(game::CBattleViewerInterf* viewer,
                          const game::CMidUnitGroup* targetGroup,
                          const game::UnitInfo* targetInfo,
                          game::UnitPositionList* targetPositions)
{
    using namespace game;

    const auto& listApi = UnitPositionListApi::get();
    const auto& groupApi = CMidUnitGroupApi::get();

    UnitPositionListIterator it, end;
    listApi.end(targetPositions, &end);
    for (listApi.begin(targetPositions, &it); !listApi.equals(&it, &end); listApi.preinc(&it)) {
        int targetPosition = listApi.dereference(&it)->first;
        if (targetPosition >= 0) {
            const CMidgardID* targetId = groupApi.getUnitIdByPosition(targetGroup, targetPosition);
            markAttackTarget(viewer, targetId, targetInfo->unitFlags.parts.attacker,
                             targetPosition);
        }
    }
}

bool isUnitSelected(game::CBattleViewerInterf* viewer,
                    const game::CMqPoint* mousePosition,
                    const game::UnitInfo* targetInfo,
                    game::CMidgardID* targetId)
{
    using namespace game;

    const auto& id = CMidgardIDApi::get();
    const auto& battle = BattleMsgDataApi::get();
    const auto& rectApi = MqRectApi::get();
    const auto& viewerApi = BattleViewerInterfApi::get();

    id.validateId(targetId, targetInfo->unitId1);

    if (battle.getUnitStatus(&viewer->data->battleMsgData, targetId, BattleStatus::Dead)
        || battle.getUnitStatus(&viewer->data->battleMsgData, targetId, BattleStatus::Retreated)
        || battle.getUnitStatus(&viewer->data->battleMsgData, targetId, BattleStatus::Hidden)) {
        return false;
    }

    bool isUnitBig = viewerApi.isUnitBig(&viewer->data->unknownUnitData, targetId);
    bool isUnitRetreating = viewerApi.isUnitRetreating(&viewer->data->unknownUnitData, targetId);
    bool isUnitOnTheLeft = viewerApi.isUnitOnTheLeft(viewer, targetInfo->unitFlags.parts.attacker);

    CMqRect unitRect{};
    viewerApi.getUnitRectPlusExtra(&unitRect,
                                   isUnitOnTheLeft ? &viewer->data->groupAreas2
                                                   : &viewer->data->groupAreas1,
                                   targetInfo->unitFlags.parts.indexInGroup, isUnitBig,
                                   isUnitRetreating);

    return rectApi.ptInRect(&unitRect, mousePosition);
}

void markAttackTargets(game::CBattleViewerInterf* viewer,
                       const game::CMidgardID* targetGroupId,
                       const game::CMidgardID* selectedUnitId,
                       const game::UnitInfo* targetInfo,
                       bool a3,
                       bool isAllAttackReach,
                       bool isBattleGoing,
                       game::UnitPositionList* targetPositions)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& groupApi = CMidUnitGroupApi::get();
    const auto& viewerApi = BattleViewerInterfApi::get();
    const auto& listApi = UnitPositionListApi::get();
    const auto& engineApi = BatViewer2DEngineApi::get();

    if (a3) {
        CBatBigFace* bigFace = viewerApi.getBigFace(viewer);
        viewerApi.setUnitId(bigFace, selectedUnitId);
    }

    if (viewer->data2->selectedUnitId == *selectedUnitId)
        return;
    engineApi.clearTargetMarks(viewer->data->batViewer2dEngine);
    viewer->data2->selectedUnitId = *selectedUnitId;

    void* tmp{};
    CMidUnitGroup* targetGroup = fn.getStackFortRuinGroup(tmp, viewer->data->objectMap,
                                                          targetGroupId);
    int selectedPositionInTargetGroup = groupApi.getUnitPosition(targetGroup, selectedUnitId);

    UnitPositionPair positionPair;
    listApi.findByPosition(&positionPair, targetPositions,
                           targetInfo->unitFlags.parts.indexInGroup);
    if (isAllAttackReach && isBattleGoing && selectedPositionInTargetGroup != -1
        && positionPair.second) {
        markAllAttackTargets(viewer, targetGroup, targetInfo, targetPositions);
    } else {
        markAttackTarget(viewer, selectedUnitId, targetInfo->unitFlags.parts.attacker,
                         targetInfo->unitFlags.parts.indexInGroup);
    }
}

bool markAttackTargets(game::CBattleViewerInterf* viewer,
                       const game::CMqPoint* mousePosition,
                       bool a3,
                       const game::IAttack* attack,
                       bool isBattleGoing,
                       const game::CMidgardID* targetGroupId,
                       game::UnitPositionList* targetPositions)
{
    using namespace game;

    const auto& battle = BattleMsgDataApi::get();
    const auto& listApi = UnitInfoListApi::get();

    bool isAllAttackReach = false;
    if (attack != nullptr) {
        auto reach = attack->vftable->getAttackReach(attack);
        isAllAttackReach = reach->id == AttackReachCategories::get().all->id;
    }

    UnitInfoList targetInfos{};
    listApi.constructor(&targetInfos);
    battle.getUnitInfos(&viewer->data->battleMsgData, &targetInfos, true);
    listApi.sort(&targetInfos);

    bool result = false;
    UnitInfoListIterator it, end;
    listApi.end(&targetInfos, &end);
    for (listApi.begin(&targetInfos, &it); !listApi.equals(&it, &end); listApi.preinc(&it)) {
        CMidgardID selectedUnitId{};
        result = isUnitSelected(viewer, mousePosition, listApi.dereference(&it), &selectedUnitId);
        if (result) {
            markAttackTargets(viewer, targetGroupId, &selectedUnitId, listApi.dereference(&it), a3,
                              isAllAttackReach, isBattleGoing, targetPositions);
            break;
        }
    }

    listApi.destructor(&targetInfos);
    return result;
}

bool unmarkAttackTargets(game::CBattleViewerInterf* viewer,
                         const game::CMqPoint* mousePosition,
                         bool a3)
{
    using namespace game;

    const auto& viewerApi = BattleViewerInterfApi::get();
    const auto& engineApi = BatViewer2DEngineApi::get();

    engineApi.clearTargetMarks(viewer->data->batViewer2dEngine);
    viewer->data2->selectedUnitId = emptyId;

    CMidgardID selectedUnitId{};
    viewerApi.getSelectedUnitId(viewerApi.isFlipped(viewer) ? viewer->data->batUnitGroup1
                                                            : viewer->data->batUnitGroup2,
                                &selectedUnitId, mousePosition);
    if (selectedUnitId == emptyId)
        return false;

    if (a3) {
        CBatBigFace* bigFace = viewerApi.getBigFace(viewer);
        viewerApi.setUnitId(bigFace, &selectedUnitId);
    }

    return true;
}

bool getTargetData(game::CBattleViewerInterf* viewer,
                   const game::IAttack** attack,
                   const game::CBattleViewerTargetData** targetData,
                   bool* isBattleGoing)
{
    using namespace game;

    const auto& fn = gameFunctions();
    const auto& battle = BattleMsgDataApi::get();

    if (viewer->dropManager.vftable->method1(&viewer->dropManager) > 0) {
        if (viewer->data->itemId == emptyId)
            return false;

        int itemIndex = battle.getLeaderEquippedBattleItemIndex(viewer->data->objectMap,
                                                                &viewer->data->unitId,
                                                                &viewer->data->battleMsgData,
                                                                &viewer->data->itemId);

        *attack = fn.getAttackById(viewer->data->objectMap, &viewer->data->itemId, 1, true);
        *targetData = &viewer->data->itemTargetData[itemIndex];
        *isBattleGoing = true;
    } else {
        *attack = fn.getAttackById(viewer->data->objectMap, &viewer->data->unitId, 1, true);
        *targetData = &viewer->data->unitTargetData;
        *isBattleGoing = (*targetData)->isBattleGoing;
    }

    return true;
}

bool __fastcall markAttackTargetsHooked(game::CBattleViewerInterf* thisptr,
                                        int /*%edx*/,
                                        const game::CMqPoint* mousePosition,
                                        bool a3)
{
    using namespace game;

    const auto& listApi = UnitPositionListApi::get();

    if (thisptr->data->unitId == emptyId)
        return false;

    const IAttack* attack = nullptr;
    const CBattleViewerTargetData* targetData = nullptr;
    bool isBattleGoing = false;
    if (!getTargetData(thisptr, &attack, &targetData, &isBattleGoing))
        return false;

    UnitPositionList targetPositions{};
    listApi.constructor(&targetPositions);
    listApi.copyAssignment(&targetPositions, &targetData->targetPositions);

    bool result = markAttackTargets(thisptr, mousePosition, a3, attack, isBattleGoing,
                                    &targetData->targetGroupId, &targetPositions);
    if (!result)
        result = unmarkAttackTargets(thisptr, mousePosition, a3);

    listApi.destructor(&targetPositions);
    return result;
}

} // namespace hooks
