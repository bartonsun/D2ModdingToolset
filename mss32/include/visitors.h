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

#ifndef VISITORS_H
#define VISITORS_H

namespace game {

struct CMidgardID;
struct IMidgardObjectMap;
struct CScenarioVisitorVftable;
struct LAttitudesCategory;
struct LItemCategory;
struct LSiteCategory;
struct LTerrainCategory;
struct CMqPoint;
struct CMidStack;
struct CMidUnit;
struct CMqPointList;

/**
 * Base for all visitor classes.
 * Visitors are used to change scenario objects or rules.
 */
struct CScenarioVisitor
{
    const CScenarioVisitorVftable* vftable;
    IMidgardObjectMap* objectMap;
};

struct CScenarioVisitorVftable
{
    void* destructor;

    /** Returns true if visitor can be applied. */
    using CanApply = bool(__thiscall*)(CScenarioVisitor* thisptr);
    CanApply canApply;

    /**
     * Applies visitor.
     * @returns true if visitor was applied successfully.
     */
    using Apply = bool(__thiscall*)(CScenarioVisitor* thisptr);
    Apply apply;
};

namespace VisitorApi {

struct Api
{
    /**
     * Changes unit hit points by specified amount.
     * Uses CVisitorChangeUnitHP.
     * Handles all necessary logic with min/max values as well as stack leader alive statuses.
     * @param[in] unitId id of unit whose hit points to change.
     * @param amount hit points change amount. Positive value heals unit, negative damages.
     * @param[in] objectMap interface used for unit search.
     * @param apply specifies whether changes should be applied.
     * @returns true if hp was changed when apply was selected. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using ChangeUnitHp = bool(__stdcall*)(const CMidgardID* unitId,
                                          int amount,
                                          IMidgardObjectMap* objectMap,
                                          int apply);
    ChangeUnitHp changeUnitHp;

    using ChangeUnitXp = bool(__stdcall*)(const CMidgardID* unitId,
                                          int amount,
                                          IMidgardObjectMap* objectMap,
                                          int apply);
    ChangeUnitHp changeUnitXp;

    using UpgradeUnit = bool(__stdcall*)(const CMidgardID* unitId,
                                         const CMidgardID* upgradeUnitImplId,
                                         const CMidgardID* groupId,
                                         IMidgardObjectMap* objectMap,
                                         int apply);
    UpgradeUnit upgradeUnit;

    /**
     * Sets maximum allowed units count for a specific group.
     * Uses CVisitorForceUnitMax.
     * Changes CMidUnitGroup::maxUnitsAllowed value.
     * @param[in] groupId id of group where unit will be summoned.
     * @param maxUnitsAllowed maximum allowed units in group. Value of -1 disables the restriction.
     * @param[in] objectMap interface used for group search.
     * @param apply specifies whether visitor should be applied.
     */
    using ForceUnitMax = bool(__stdcall*)(const CMidgardID* groupId,
                                          int maxUnitsAllowed,
                                          IMidgardObjectMap* objectMap,
                                          int apply);
    ForceUnitMax forceUnitMax;

    /**
     * Adds unit to the group.
     * Uses CVisitorAddUnitToGroup.
     * @param[in] unitId id of the exising unit to add.
     * @param[in] groupId id of the group in stack, fortification or ruin to add unit to.
     * @param position unit position in group.
     * @param[in] creationTurn number of game turn when unit was created.
     * @param a5 meaning unknown, summon attack logic sets it to 1.
     * @param objectMap interface used for group search.
     * @param apply specifies whether unit addition should be applied.
     * @returns true if unit was added when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using AddUnitToGroup = bool(__stdcall*)(const CMidgardID* unitId,
                                            const CMidgardID* groupId,
                                            int position,
                                            int* creationTurn,
                                            char a5,
                                            IMidgardObjectMap* objectMap,
                                            int apply);
    AddUnitToGroup addUnitToGroup;

    /**
     * Transfers item from one object with inventory (stack, bag or city) to another.
     * Uses CVisitorExchangeItem.
     * @param[in] fromObjectId id of the object with inventory to transfer from.
     * @param[in] toObjectId id of the object with inventory to receive item.
     * @param[in] itemId id of the item to transfer.
     * @param objectMap interface used for objects search.
     * @param apply specifies whether item transfer should be applied.
     * @returns true if item was transfered when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using ExchangeItem = bool(__stdcall*)(const CMidgardID* fromObjectId,
                                          const CMidgardID* toObjectId,
                                          const CMidgardID* itemId,
                                          IMidgardObjectMap* objectMap,
                                          int apply);
    ExchangeItem exchangeItem;

    /**
     * Transforms a unit to another.
     * Uses CVisitorTransformUnit.
     * @param[in] unitId id of the unit to be transformed.
     * @param[in] toUnitImplId id of the unit impl to transform to.
     * @param[in] keepHp specifies whether the transformed unit retains its original hp.
     * @param objectMap interface used for objects search.
     * @param apply specifies whether unit transformation should be applied.
     * @returns true if unit was transformed when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using TransformUnit = bool(__stdcall*)(CMidgardID* unitId,
                                           CMidgardID* toUnitImplId,
                                           bool keepHp,
                                           IMidgardObjectMap* objectMap,
                                           int apply);
    TransformUnit transformUnit;

    /**
     * Untransforms a unit.
     * Uses CVisitorUndoTransformUnit.
     * @param[in] unitId id of the unit to be untransformed.
     * @param objectMap interface used for objects search.
     * @param apply specifies whether unit transformation should be applied.
     * @returns true if unit was untransformed when apply set to 1. If apply set to 0, returns
     * whether visitor can be applied.
     */
    using UndoTransformUnit = bool(__stdcall*)(CMidgardID* unitId,
                                               IMidgardObjectMap* objectMap,
                                               int apply);
    UndoTransformUnit undoTransformUnit;

    /**
     * Extracts unit from group.
     * Uses CVisitorExtractUnitFromGroup.
     * @param[in] unitId id of unit to extract.
     * @param[in] groupId id of group to extract from.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether unit extraction should be applied.
     * @returns true if unit was extracted when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using ExtractUnitFromGroup = bool(__stdcall*)(const CMidgardID* unitId,
                                                  const CMidgardID* groupId,
                                                  IMidgardObjectMap* objectMap,
                                                  int apply);
    ExtractUnitFromGroup extractUnitFromGroup;

    /**
     * Reinserts unit back into group.
     * Uses CVisitorReinsertUnitIntoGroup.
     * @param[in] unitId id of unit to reinsert.
     * @param[in] groupId id of group to reinsert into.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether unit reinsertion should be applied.
     * @returns true if unit was reinserted when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using ReinsertUnitIntoGroup = bool(__stdcall*)(const CMidgardID* unitId,
                                                   const CMidgardID* groupId,
                                                   int position,
                                                   IMidgardObjectMap* objectMap,
                                                   int apply);
    ReinsertUnitIntoGroup reinsertUnitIntoGroup;

    /**
    * Swap unit position. Can't swap if firstPosition is empty and secondPosition is a small unit.
    * Uses CVisitorSwapUnitPosition.
    * @param firstPosition unit position in group.
    * @param[in] stackId id of group. Can be stack->id, fort->id, ruin->id.
    * @param firstPosition position in group.
    * @param[in] otherStackId id of group. Can be stack->id, fort->id, ruin->id.
    * @param[in] objectMap interface used for objects search.
    * @returns true if player attitude was changed when apply set to 1. If apply set to 0, returns
    * whether visitor can be applied. 
    **/
    using SwapUnitPosition = bool(__stdcall*)(int firstPosition, const CMidgardID* stackId, int secondPosition, const CMidgardID* otherStackId, IMidgardObjectMap* objectMap, int apply);
    SwapUnitPosition swapUnitPosition;

    /**
     * Revives a dead unit in a stack.
     * Uses CVisitorReviveUnit.
     * @param[in] unitId id of unit to revive.
     * @param[in] objectMap interface used for unit search.
     * @param apply specifies whether changes should be applied.
     * @returns true if unit was revived when apply was selected. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using ReviveUnit = bool(__stdcall*)(const CMidgardID* unitId,
                                        IMidgardObjectMap* objectMap,
                                        int apply);
    ReviveUnit reviveUnit;


    /**
    * Create item and add to inventory.
    * Uses CVisitorCreateItem.
    * @param[in] itemImplId id of the item impl to create.
    * @param[in] ownerId id of the owner. It can be stack, fort, bag.
    * @param[in] objectMap interface used for objects search.
    * @param apply specifies whether unit reinsertion should be applied.
    * @returns true if unit was reinserted when apply set to 1. If apply set to 0, returns whether
    * visitor can be applied.
    **/
    using CreateItem = bool(__stdcall*)(const CMidgardID* ownerId,
                                        const CMidgardID* itemImplId,
                                        IMidgardObjectMap* objectMap,
                                        int apply);
    CreateItem createItem;

    /**
     * Destroys an item from an object's inventory (stack, bag or city).
     * Uses CVisitorDestroyItem.
     * @param[in] ownerId id of the object whose inventory contains the item.
     * @param[in] itemId id of the item to destroy.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether item destruction should be applied.
     * @returns true if item was destroyed when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using DestroyItem = bool(__stdcall*)(const CMidgardID* ownerId,
                                         const CMidgardID* itemId,
                                         IMidgardObjectMap* objectMap,
                                         int apply);
    DestroyItem destroyItem;

    /**
     * Adds an item to a merchant's inventory for sale.
     * Uses CVisitorMerchantAddItem.
     * @param[in] merchantId id of the merchant site.
     * @param[in] itemId id of the item to add for sale.
     * @param amount amount of items to add.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether item addition should be applied.
     * @returns true if item was added when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using MerchantAddItem = bool(__stdcall*)(const CMidgardID* merchantId,
                                             const CMidgardID* itemImplId,
                                             int amount,
                                             IMidgardObjectMap* objectMap,
                                             int apply);
    MerchantAddItem merchantAddItem;

    /**
     * Removes an item from a merchant's inventory.
     * Uses CVisitorMerchantDelItem.
     * @param[in] merchantId id of the merchant site.
     * @param[in] itemId id of the item to remove from sale.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether item removal should be applied.
     * @returns true if item was removed when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using MerchantDelItem = bool(__stdcall*)(const CMidgardID* merchantId,
                                             const CMidgardID* itemImplId,
                                             IMidgardObjectMap* objectMap,
                                             int apply);
    MerchantDelItem merchantDelItem;

    /**
     * Creates a rod (territory capture rod) at the specified position.
     * Uses CVisitorCreateRod.
     * @param[in] playerId id of the player who will own the rod.
     * @param[in] position position on the map where to place the rod.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether rod creation should be applied.
     * @returns true if rod was created when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using CreateRod = bool(__stdcall*)(const CMidgardID* ownerId, 
                                       const CMqPoint* position, 
                                       IMidgardObjectMap* objectMap, 
                                       int apply);
    CreateRod createRod;

    /**
     * Destroys a rod (territory capture rod) on the map.
     * Uses CVisitorDestroyRod.
     * @param[in] rodId id of the rod to destroy.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether rod destruction should be applied.
     * @returns true if rod was destroyed when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using DestroyRod = bool(__stdcall*)(const CMidgardID* rodId,
                                        IMidgardObjectMap* objectMap,
                                        int apply);
    DestroyRod destroyRod;

    /**
     * Moves a stack to the specified position on the map.
     * Uses CVisitorMoveStack.
     * @param[in] stackId id of the stack to move.
     * @param[in] position target position on the map.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether movement should be applied.
     * @returns true if stack was moved when apply set to 1. If apply set to 0, returns whether
     * visitor can be applied.
     */
    using MoveStack = bool(__stdcall*)(const CMidgardID* stackId,
                                       const CMqPoint* position,
                                       IMidgardObjectMap* objectMap,
                                       int apply);
    MoveStack moveStack;

    /**
     * Changes stack movement points by specified delta.
     * Uses CVisitorChangeStackMoveAllowance.
     * Positive delta decreases movement points, negative delta increases them.
     * @param[in] stackId id of stack whose movement points to change.
     * @param movementDelta movement points change amount. Positive value decreases, negative
     * increases.
     * @param[in] objectMap interface used for stack search.
     * @param apply specifies whether changes should be applied.
     * @returns true if movement points were changed when apply was selected. If apply set to 0,
     * returns whether visitor can be applied.
     */
    using ChangeStackMoveAllowance = bool(__stdcall*)(const CMidgardID* stackId, int movement, IMidgardObjectMap* objectMap, int apply);
    ChangeStackMoveAllowance changeStackMoveAllowance;

    using ChangeMapTerrain = bool(__stdcall*)(const LTerrainCategory* terrainCategory,
                                              CMqPointList* positions,
                                              int* a3,
                                              IMidgardObjectMap* objectMap,
                                              int apply);
    ChangeMapTerrain changeMapTerrain;

    using RunKillStack = bool(__stdcall*)(const CMidgardID* stackId,
                                          const CMidgardID* killerId,
                                          const CMidgardID* playerId,
                                          const CMidgardID* deathSourceId,
                                          int flags,
                                          bool destroyItems,
                                          IMidgardObjectMap* objectMap,
                                          int apply);
    RunKillStack runKillStack;

    using CreateStackDestroyed = bool(__stdcall*)(const CMidgardID* stackId,
                                                     const int* posX,
                                                     const int* posY,
                                                     IMidgardObjectMap* objectMap,
                                                     int apply);
    CreateStackDestroyed createStackDestroyed;

    using OverlayUnit = bool(__stdcall*)(const CMidgardID* unitId,
                                         CMidUnit* sourceUnit,
                                         IMidgardObjectMap* objectMap,
                                         int apply);
    OverlayUnit overlayUnit;

    /**
     * Changes player attitude.
     * Uses CVisitorPlayerSetAttitude.
     * Can be used only in Scenario Editor.
     * @param[in] playerId id of player to change.
     * @param[in] attitude new attitude category to set.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether attitude should be changed.
     * @returns true if player attitude was changed when apply set to 1. If apply set to 0, returns
     * whether visitor can be applied.
     */
    using PlayerSetAttitude = bool(__stdcall*)(const CMidgardID* playerId,
                                               const LAttitudesCategory* attitude,
                                               IMidgardObjectMap* objectMap,
                                               int apply);
    PlayerSetAttitude playerSetAttitude;

    /**
     * Sets source template id for a stack.
     * Uses CVisitorSetStackSrcTemplate.
     * @param[in] stackId id of stack to change.
     * @param[in] stackTemplateId source template id to set.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether stack source template should be changed.
     * @returns true if source template was set when apply set to 1. If apply set to 0, returns
     * whether visitor can be applied.
     */
    using SetStackSrcTemplate = bool(__stdcall*)(const CMidgardID* stackId,
                                                 const CMidgardID* stackTemplateId,
                                                 IMidgardObjectMap* objectMap,
                                                 int apply);
    SetStackSrcTemplate setStackSrcTemplate;

    /**
     * Allows merchant to buy items of specified category.
     * Can be called only in Scenario Editor.
     * Uses CVisitorMerchantAddBuyCategory.
     * @param[in] siteId id of merchant to change.
     * @param[in] itemCategory category of items merchant is allowed to buy.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether merchant object should be changed.
     * @returns true if merchant was changed successfully when apply set to 1. If ally set to 0,
     * returns whether visitor can be applied.
     */
    using MerchantAddBuyCategory = bool(__stdcall*)(const CMidgardID* siteId,
                                                    const LItemCategory* itemCategory,
                                                    IMidgardObjectMap* objectMap,
                                                    int apply);
    MerchantAddBuyCategory merchantAddBuyCategory;

    /**
     * Creates site of specified category.
     * Can be called only in Scenario Editor.
     * Uses CVisitorCreateSite.
     * @param[in] site site category to create.
     * @param[in] mapPosition position on a map where to create a new site.
     * @param imgIso site scenario map image index.
     * @param[in] imgIntf site interface image.
     * @param[in] name site name.
     * @param[in] description site description.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether site object should be created.
     * @returns true if site was creates successfully when apply set to 1. If apply set to 0,
     * returns wheter visitor can be applied.
     */
    using CreateSite = bool(__stdcall*)(const LSiteCategory* site,
                                        const CMqPoint* mapPosition,
                                        int imgIso,
                                        const char* imgIntf,
                                        const char* name,
                                        const char* description,
                                        IMidgardObjectMap* objectMap,
                                        int apply);
    CreateSite createSite;

    /**
     * Changes site object information.
     * Can be called only in Scenario Editor.
     * Uses CVisitorChangeSiteInfo.
     * @param[in] siteId id of existing site object to change.
     * @param[in] name new name for a site.
     * @param[in] description new description for a site.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether site object info should be changed.
     * @returns true if site info was changed successfully when apply set to 1.
     * If apply set to 0, returns wheter visitor can be applied.
     */
    using ChangeSiteInfo = bool(__stdcall*)(const CMidgardID* siteId,
                                            const char* name,
                                            const char* description,
                                            IMidgardObjectMap* objectMap,
                                            int apply);
    ChangeSiteInfo changeSiteInfo;

    /**
     * Changes site object image on a strategic map.
     * Can be called only in Scenario Editor.
     * Uses CVisitorChangeSiteImage.
     * @param[in] siteId id of existing site object to change.
     * @param imgIso strategic map image index.
     * @param[in] objectMap interface used for objects search.
     * @param apply specifies whether site object image should be changed.
     * @returns true if site image was changed successfully when apply set to 1.
     * If apply set to 0, returns whether visitor can be applied.
     */
    using ChangeSiteImage = bool(__stdcall*)(const CMidgardID* siteId,
                                             int imgIso,
                                             IMidgardObjectMap* objectMap,
                                             int apply);
    ChangeSiteImage changeSiteImage;

    using ChangeSiteAiPriority = bool(__stdcall*)(const CMidgardID* siteId,
                                                  int aiPriority,
                                                  IMidgardObjectMap* objectMap,
                                                  int apply);
    ChangeSiteAiPriority changeSiteAiPriority;

    using ChangeStackLeader = bool(__stdcall*)(CMidgardID* stackId,
                                               CMidgardID* unitId,
                                               IMidgardObjectMap* objectMap,
                                               int apply);
    ChangeStackLeader changeStackLeader;
};

Api& get();

} // namespace VisitorApi
} // namespace game

#endif // VISITORS_H
