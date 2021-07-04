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

#ifndef BATTLEVIEWERINTERF_H
#define BATTLEVIEWERINTERF_H

#include "attackclasscat.h"
#include "avoidflickerimage.h"
#include "battlemsgdata.h"
#include "batviewer.h"
#include "batviewerutils.h"
#include "d2string.h"
#include "d2vector.h"
#include "draganddropinterf.h"
#include "linkedlist.h"
#include "midgardid.h"
#include "mqrect.h"
#include "sortedlist.h"
#include "unitpositionlist.h"
#include <cstddef>

namespace game {

struct BattleAttackInfo;
struct CBatViewer2DEngine;
struct CBatImagesLoader;
struct CBatBigFace;
struct CBatUnitGroup2;
struct CBatUnitAnim;
struct CMidgardMsgBox;
struct IMidgardObjectMap;
struct CBatEncyclopediaInterf;
struct CBatLog;

struct CBattleViewerGroupAreas
{
    CMqRect unitImageAreas[6];
    CMqRect unitBgndImageArea;
    String unitBgndString;
    CMqRect slotImageAreas[6];
    CMqRect meterImageAreas[6];
    CMqRect bigFaceImageArea;
    CMqRect bigTextImageArea;
    CMqRect itemImageAreas[2];
    CMqRect itemBgndImageArea;
    String itemBgndString;
};

static_assert(sizeof(CBattleViewerGroupAreas) == 416,
              "Size of CBattleViewerGroupAreas structure must be exactly 416 bytes");

struct CBattleViewerUnknownData
{
    int unknown;
    int unknown2;
    int unknown3;
    int unknown4;
};

static_assert(sizeof(CBattleViewerUnknownData) == 16,
              "Size of CBattleViewerUnknownData structure must be exactly 16 bytes");

struct CBattleViewerUnknown
{
    CBattleViewerUnknownData data;
    CBattleViewerUnknownData data2[6];
    CBattleViewerUnknownData data3[6];
    String string;
    SortedList<void> list; /** < Each node contains 16 bytes of data. */
};

static_assert(offsetof(CBattleViewerUnknown, data3) == 112,
              "CBattleViewerUnknown::data3 offset must be 112 bytes");

static_assert(offsetof(CBattleViewerUnknown, string) == 208,
              "CBattleViewerUnknown::string offset must be 208 bytes");

struct CBattleViewerTargetData
{
    bool isBattleGoing;
    bool unknown;
    char padding[2];
    CMidgardID targetGroupId;
    UnitPositionList targetPositions;
};

static_assert(sizeof(CBattleViewerTargetData) == 36,
              "Size of CBattleViewerTargetData structure must be exactly 36 bytes");

struct CBattleViewerTargetDataSet
{
    CBattleViewerTargetData attack;
    CBattleViewerTargetData items[2];
};

static_assert(sizeof(CBattleViewerTargetDataSet) == 108,
              "Size of CBattleViewerTargetDataSet structure must be exactly 108 bytes");

struct CBattleViewerUnknownUnitData
{
    CMidgardID unknownId;
    int unknown;
    bool unknown2;
    bool isUnitBig;
    bool isUnitRetreating;
    char padding;
};

static_assert(sizeof(CBattleViewerUnknownUnitData) == 12,
              "Size of CBattleViewerUnknownUnitData structure must be exactly 12 bytes");

using CUnknownUnitDataList = SortedList<Pair<CMidgardID, CBattleViewerUnknownUnitData>>;

struct CBattleViewerInterfData
{
    CAvoidFlickerImage avoidFlickerImage;
    char unknown[4];
    BattleMsgData battleMsgData;
    CUnknownUnitDataList unknownUnitData;
    CMidgardID unitId;
    CBattleViewerTargetDataSet targetData;
    BattleAttackInfo** attackInfo;
    char unknown3[4];
    CMidgardID itemId;
    Vector<void*> unknownArray; /**< Each element contains 32 bytes of data. */
    CBatViewer2DEngine* batViewer2dEngine;
    CMqRect dialogInterfArea;
    CBatImagesLoader* imagesLoader;
    CBattleViewerGroupAreas groupAreas1;
    CBattleViewerGroupAreas groupAreas2;
    CBatBigFace* bigFace;
    CBatUnitGroup2* batUnitGroup1;
    CBatUnitGroup2* batUnitGroup2;
    char unknown4[8];
    CBatBigFace* bigFace2;
    CBatUnitGroup2* batUnitGroup3;
    CBatUnitGroup2* batUnitGroup4;
    char unknown41[8];
    CBatUnitAnim* unitAnimations[6];
    CBatUnitAnim* unitAnimations2[6];
    CBattleViewerUnknown unknown11;
    char unknown5[4];
    CBatViewerUtils::CAnimCounter animCounter;
    char unknown10[4];
    CMqRect pUnitGroupSpotArea;
    bool unknown7;
    bool unknown8;
    bool unknown9;
    bool flippedBattle;
    bool bothPlayersHuman;
    char unknown6[3];
    CMidgardMsgBox* messageBox;
    IMidgardObjectMap* objectMap;
    bool stickyEncy;
    char padding[3];
};

static_assert(sizeof(CBattleViewerInterfData) == 5384,
              "Size of CBattleViewerInterfData structure must be exactly 5384 bytes");

static_assert(offsetof(CBattleViewerInterfData, batViewer2dEngine) == 4136,
              "CBattleViewerInterfData::batViewer2dEngine offset must be 4136 bytes");

static_assert(offsetof(CBattleViewerInterfData, imagesLoader) == 4156,
              "CBattleViewerInterfData::imagesLoader offset must be 4156 bytes");

static_assert(offsetof(CBattleViewerInterfData, bigFace) == 4992,
              "CBattleViewerInterfData::bigFace offset must be 4992 bytes");

static_assert(offsetof(CBattleViewerInterfData, bigFace2) == 5012,
              "CBattleViewerInterfData::bigFace2 offset must be 5012 bytes");

static_assert(offsetof(CBattleViewerInterfData, bothPlayersHuman) == 5368,
              "CBattleViewerInterfData::bothPlayersHuman offset must be 5368 bytes");

static_assert(offsetof(CBattleViewerInterfData, objectMap) == 5376,
              "CBattleViewerInterfData::objectMap offset must be 5376 bytes");

struct CBattleViewerInterfData2
{
    char* dialogName;
    int battleSpeed;
    bool battleAnimations;
    bool battleSpeedInstant;
    char padding[2];
    CMidgardID unknownId;
    LAttackClass attackClass;
    char unknown[32];
    CBatEncyclopediaInterf* encyclopedia;
    char unknown1[48];
    CBatLog* log;
    LinkedList<void*> list; /**< Nodes unknown. */
    LinkedList<void*> list2;
    LinkedList<void*> list3;
    char unknown2[8];
    CMidgardID selectedUnitId;
    LinkedList<void*> list4; /**< Each node contains 16 bytes of data. */
    int unknown3;
};

static_assert(sizeof(CBattleViewerInterfData2) == 196,
              "Size of CBattleViewerInterfData2 structure must be exactly 196 bytes");

/**
 * Renders battle screen and handles user input during battle.
 * Represents DLG_BATTLE_A from Interf.dlg.
 */
struct CBattleViewerInterf : public CDragAndDropInterf
{
    IBatViewer batViewer;
    CBattleViewerInterfData* data;
    CBattleViewerInterfData2* data2;
};

static_assert(sizeof(CBattleViewerInterf) == 36,
              "Size of CBattleViewerInterf structure must be exactly 36 bytes");

namespace BattleViewerInterfApi {

struct Api
{
    using MarkAttackTargets = bool(__thiscall*)(CBattleViewerInterf* thisptr,
                                                const CMqPoint* mousePosition,
                                                bool setBigFace);
    MarkAttackTargets markAttackTargets;

    using IsUnitOnTheLeft = bool(__thiscall*)(const CBattleViewerInterf* thisptr, bool isAttacker);
    IsUnitOnTheLeft isUnitOnTheLeft;

    using IsFlipped = bool(__thiscall*)(const CBattleViewerInterf* thisptr);
    IsFlipped isFlipped;

    using GetBigFace = CBatBigFace*(__thiscall*)(const CBattleViewerInterf* thisptr);
    GetBigFace getBigFace;

    using GetUnitRect = CMqRect*(__stdcall*)(CMqRect* value,
                                             CBattleViewerGroupAreas* groupAreas,
                                             int unitPosition,
                                             bool isUnitBig,
                                             bool isUnitRetreating);
    GetUnitRect getUnitRect;
    GetUnitRect getUnitRectPlusExtra;

    using GetBoolById = bool(__thiscall*)(CUnknownUnitDataList* thisptr, const CMidgardID* unitId);
    GetBoolById isUnitBig;
    GetBoolById isUnitRetreating;

    using SetUnitId = void(__thiscall*)(CBatBigFace* thisptr, const CMidgardID* unitId);
    SetUnitId setUnitId;

    using GetSelectedUnitId = CMidgardID*(__thiscall*)(const CBatUnitGroup2* thisptr,
                                                       CMidgardID* value,
                                                       const CMqPoint* mousePosition);
    GetSelectedUnitId getSelectedUnitId;
};

Api& get();

const IBatViewerVftable* vftable();

} // namespace BattleViewerInterfApi

} // namespace game

#endif // BATTLEVIEWERINTERF_H
