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


#include "button.h"
#include "listbox.h"
#include "scenarioview.h"
#include "scenarioinfo.h"
#include "fortview.h"
#include "menubase.h"
#include "itemtransferhooks.h"
#include "citystackinterf.h"
#include "dialoginterf.h"
#include "dynamiccast.h"
#include "exchangeinterf.h"
#include "fortification.h"
#include "globaldata.h"
#include "interfmanager.h"
#include "itembase.h"
#include "itemcategory.h"
#include "itemutils.h"
#include "mempool.h"
#include "midbag.h"
#include "midgardmsgbox.h"
#include "midgardobjectmap.h"
#include "miditem.h"
#include "midmsgboxbuttonhandlerstd.h"
#include "midstack.h"
#include "netmessages.h"
#include "originalfunctions.h"
#include "phasegame.h"
#include "pickupdropinterf.h"
#include "sitemerchantinterf.h"
#include "textids.h"
#include "utils.h"
#include "visitors.h"
#include <optional>
#include <spdlog/spdlog.h>
#include <vector>
#include <gameutils.h>

namespace hooks {

using ItemFilter = std::function<bool(game::IMidgardObjectMap* objectMap,
                                      const game::CMidgardID* itemId)>;

static const game::LItemCategory* getItemCategoryById(game::IMidgardObjectMap* objectMap,
                                                      const game::CMidgardID* itemId)
{
    auto globalItem = getGlobalItemById(objectMap, itemId);
    return globalItem ? globalItem->vftable->getCategory(globalItem) : nullptr;
}

static game::CFortification* getFortByCityId(game::IMidgardObjectMap* map,
                                             const game::CMidgardID* cityId)
{
    using namespace game;

    if (!map || !cityId)
        return nullptr;

    auto* obj = map->vftable->findScenarioObjectById(map, cityId);
    if (!obj)
        return nullptr;

    return static_cast<CFortification*>(obj);
}

static game::CMidgardID findFirstEmptyStackId(game::IMidgardObjectMap* map)
{
    using namespace game;
    if (!map)
        return invalidId;

    auto& idApi = CMidgardIDApi::get();

    IteratorPtr itBegin, itEnd;
    map->vftable->begin(map, &itBegin);
    map->vftable->end(map, &itEnd);

    IMidgardObjectMap::Iterator* it = itBegin.data;
    IMidgardObjectMap::Iterator* endIt = itEnd.data;

    if (!it || !endIt)
        return invalidId;

    for (; !it->vftable->end(it, endIt); it->vftable->advance(it)) {
        CMidgardID* objId = it->vftable->getObjectId(it);
        if (!objId)
            continue;

        if (idApi.getType(objId) == IdType::Stack) {
            auto obj = map->vftable->findScenarioObjectById(map, objId);
            if (!obj)
                continue;

            auto* stack = static_cast<CMidStack*>(obj);
            int total = stack->inventory.vftable->getItemsCount(&stack->inventory);
            if (total == 0) {
                spdlog::info("Found EMPTY Stack id={}", idToString(objId));
                return *objId;
            }
        }
    }

    spdlog::warn("No empty Stack found in scenario objects!");
    return invalidId;
}
static game::CMidgardID findFirstEmptyBagId(game::IMidgardObjectMap* map)
{
    using namespace game;
    if (!map)
        return invalidId;

    auto& idApi = CMidgardIDApi::get();

    IteratorPtr itBegin, itEnd;
    map->vftable->begin(map, &itBegin);
    map->vftable->end(map, &itEnd);

    IMidgardObjectMap::Iterator* it = itBegin.data;
    IMidgardObjectMap::Iterator* endIt = itEnd.data;

    if (!it || !endIt)
        return invalidId;

    for (; !it->vftable->end(it, endIt); it->vftable->advance(it)) {
        CMidgardID* objId = it->vftable->getObjectId(it);
        if (!objId)
            continue;

        if (idApi.getType(objId) == IdType::Bag) {
            auto obj = map->vftable->findScenarioObjectById(map, objId);
            if (!obj)
                continue;

            auto* bag = static_cast<CMidBag*>(obj);
            int total = bag->inventory.vftable->getItemsCount(&bag->inventory);
            if (total == 0) {
                spdlog::info("Found EMPTY Bag id={}", idToString(objId));
                return *objId;
            }
        }
    }

    spdlog::warn("No empty Bag found in scenario objects!");
    return invalidId;
}



#define CAT_MATCHER(name, field)                                                                   \
    static bool name(game::IMidgardObjectMap* m, const game::CMidgardID* id)                       \
    {                                                                                              \
        auto c = getItemCategoryById(m, id);                                                       \
        return c && game::ItemCategories::get().field                                              \
               && c->id == game::ItemCategories::get().field->id;                                  \
    }

CAT_MATCHER(isArmor, armor)
CAT_MATCHER(isJewel, jewel)
CAT_MATCHER(isWeapon, weapon)
CAT_MATCHER(isBanner, banner)
CAT_MATCHER(isPotionBoost, potionBoost)
CAT_MATCHER(isPotionHeal, potionHeal)
CAT_MATCHER(isPotionRevive, potionRevive)
CAT_MATCHER(isPotionPermanent, potionPermanent)
CAT_MATCHER(isScroll, scroll)
CAT_MATCHER(isWand, wand)
CAT_MATCHER(isValuable, valuable)
CAT_MATCHER(isOrb, orb)
CAT_MATCHER(isTalisman, talisman)
CAT_MATCHER(isTravel, travelItem)
CAT_MATCHER(isSpecial, special)
#undef CAT_MATCHER



static bool isPotion(game::IMidgardObjectMap* objectMap, const game::CMidgardID* itemId)
{
    using namespace game;

    auto category = getItemCategoryById(objectMap, itemId);
    if (!category) {
        return false;
    }

    const auto& categories = ItemCategories::get();
    const auto id = category->id;

    return categories.potionBoost->id == id || categories.potionHeal->id == id
           || categories.potionPermanent->id == id || categories.potionRevive->id == id;
}

static bool isSpell(game::IMidgardObjectMap* objectMap, const game::CMidgardID* itemId)
{
    using namespace game;

    auto category = getItemCategoryById(objectMap, itemId);
    if (!category) {
        return false;
    }

    const auto& categories = ItemCategories::get();
    const auto id = category->id;

    return categories.scroll->id == id || categories.wand->id == id;
}

/** Transfers items from src object to dst. */
static void transferItems(const std::vector<game::CMidgardID>& items,
                          game::CPhaseGame* phaseGame,
                          const game::CMidgardID* dstObjectId,
                          const char* dstObjectName,
                          const game::CMidgardID* srcObjectId,
                          const char* srcObjectName)
{
    using namespace game;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    const auto& exchangeItem = VisitorApi::get().exchangeItem;
    const auto& sendExchangeItemMsg = NetMessagesApi::get().sendStackExchangeItemMsg;

    for (const auto& item : items) {
        sendExchangeItemMsg(phaseGame, srcObjectId, dstObjectId, &item, 0);

        if (!exchangeItem(srcObjectId, dstObjectId, &item, objectMap, 0)) {
            spdlog::error("Failed to transfer item {:s} from {:s} {:s} to {:s} {:s}",
                          idToString(&item), srcObjectName, idToString(srcObjectId), dstObjectName,
                          idToString(dstObjectId));
        }
    }
}

/** Transfers city items to visiting stack. */
static void transferCityToStack(game::CPhaseGame* phaseGame,
                                const game::CMidgardID* cityId,
                                std::optional<ItemFilter> itemFilter = std::nullopt)
{
    using namespace game;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, cityId);
    if (!obj) {
        spdlog::error("Could not find city {:s}", idToString(cityId));
        return;
    }

    auto fortification = static_cast<CFortification*>(obj);
    if (fortification->stackId == emptyId) {
        return;
    }

    auto& inventory = fortification->inventory;
    std::vector<CMidgardID> items;
    const int itemsTotal = inventory.vftable->getItemsCount(&inventory);
    for (int i = 0; i < itemsTotal; ++i) {
        auto item = inventory.vftable->getItem(&inventory, i);
        if (!itemFilter || (itemFilter && (*itemFilter)(objectMap, item))) {
            items.push_back(*item);
        }
    }

    transferItems(items, phaseGame, &fortification->stackId, "stack", cityId, "city");
}

/** Sorts city inventory by given filter (matching items go first). */
static void sortCity(game::CPhaseGame* phaseGame, const game::CMidgardID* cityId, ItemFilter filter)
{
    using namespace game;

    auto* objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    if (!objectMap)
        return;

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, cityId);
    if (!obj) {
        spdlog::error("Could not find city {:s}", idToString(cityId));
        return;
    }

    auto fortification = static_cast<CFortification*>(obj);
    if (fortification->stackId == emptyId) {
        return;
    }

    // Собираем все предметы в городе
    std::vector<CMidgardID> items;
    const int total = fortification->inventory.vftable->getItemsCount(&fortification->inventory);
    for (int i = 0; i < total; ++i) {
        if (auto id = fortification->inventory.vftable->getItem(&fortification->inventory, i)) {
            items.push_back(*id);
        }
    }

    // Сохраняем исходный порядок
    auto original = items;

    // Сортируем: подходящие фильтру вперёд
    std::stable_partition(items.begin(), items.end(),
                          [&](const CMidgardID& id) { return filter(objectMap, &id); });

    // Проверка: изменился ли порядок
    if (items == original) {
        spdlog::debug("City {:s}: already sorted by filter, skipping transfer", idToString(cityId));
        return;
    }

    // Найдём первый стек с пустым инвентарём
    CMidgardID stackId = fortification->stackId;
    if (stackId == invalidId) {
        spdlog::error("No empty stack found, using fortification->stackId instead");
        return;
    }

    // город -> стек
    transferItems(items, phaseGame, &stackId, "stack", cityId, "city");

    // стек -> город
    transferItems(items, phaseGame, cityId, "city", &stackId, "stack");
}

#define CITY_SORT_MATCHER(fnName, matcher)                                                         \
    void __fastcall fnName(game::CCityStackInterf* thisptr, int)                                   \
    {                                                                                              \
        sortCity(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId, matcher);     \
    }
CITY_SORT_MATCHER(cityInterfSortArmor, isArmor)
CITY_SORT_MATCHER(cityInterfSortJewel, isJewel)
CITY_SORT_MATCHER(cityInterfSortWeapon, isWeapon)
CITY_SORT_MATCHER(cityInterfSortBanner, isBanner)
CITY_SORT_MATCHER(cityInterfSortPotionBoost, isPotionBoost)
CITY_SORT_MATCHER(cityInterfSortPotionHeal, isPotionHeal)
CITY_SORT_MATCHER(cityInterfSortPotionRevive, isPotionRevive)
CITY_SORT_MATCHER(cityInterfSortPotionPermanent, isPotionPermanent)
CITY_SORT_MATCHER(cityInterfSortScroll, isScroll)
CITY_SORT_MATCHER(cityInterfSortWand, isWand)
CITY_SORT_MATCHER(cityInterfSortValuable, isValuable)
CITY_SORT_MATCHER(cityInterfSortOrb, isOrb)
CITY_SORT_MATCHER(cityInterfSortTalisman, isTalisman)
CITY_SORT_MATCHER(cityInterfSortTravel, isTravel)
CITY_SORT_MATCHER(cityInterfSortSpecial, isSpecial)


static std::optional<bindings::PlayerView> getFortOwner(game::CFortification* fort,
                                                        game::IMidgardObjectMap* map)
{
    if (!fort || !map)
        return std::nullopt;
    bindings::FortView view(fort, map);
    return view.getOwner();
}

static bool isSameOwner(const std::optional<bindings::PlayerView>& a,
                        const std::optional<bindings::PlayerView>& b)
{
    if (!a || !b)
        return false;

    const auto id1 = a->getId().id;
    const auto id2 = b->getId().id;

    return memcmp(&id1, &id2, sizeof(game::CMidgardID)) == 0;
}

static game::CFortification* findCapital(game::IMidgardObjectMap* map,
                                         const std::optional<bindings::PlayerView>& owner)
{
     using namespace game;
    if (!map || !owner)
        return nullptr;

    IteratorPtr itBegin, itEnd;
    map->vftable->begin(map, &itBegin);
    map->vftable->end(map, &itEnd);

    IMidgardObjectMap::Iterator* it = itBegin.data;
    IMidgardObjectMap::Iterator* endIt = itEnd.data;

    for (; !it->vftable->end(it, endIt); it->vftable->advance(it)) {
        CMidgardID* objId = it->vftable->getObjectId(it);
        if (!objId)
            continue;

        auto* obj = map->vftable->findScenarioObjectById(map, objId);
        if (!obj || CMidgardIDApi::get().getType(objId) != IdType::Fortification)
            continue;

        auto* f = static_cast<game::CFortification*>(obj);
        bindings::FortView view(f, map);
        if (!view.isCapital())
            continue;

        auto capOwner = view.getOwner();
        if (isSameOwner(capOwner, owner))
            return f;
    }

    return nullptr;
}

static std::vector<game::CMidgardID> collectFortItems(game::CFortification* fort)
{
    std::vector<game::CMidgardID> items;
    if (!fort)
        return items;

    int total = fort->inventory.vftable->getItemsCount(&fort->inventory);
    for (int i = 0; i < total; ++i) {
        if (auto id = fort->inventory.vftable->getItem(&fort->inventory, i))
            items.push_back(*id);
    }
    return items;
}

/** Transfers city items to its capital  */
static void transferCityToCapital(game::CPhaseGame* phaseGame,
                                  const game::CMidgardID* cityId,
                                  std::optional<ItemFilter> filter = std::nullopt)
{
    using namespace game;

    static std::unordered_map<std::string, int> lastTurnByOwner;

    auto* objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    if (!objectMap)
        return;

    auto* info = hooks::getScenarioInfo(objectMap);
    if (!info)
        return;

    int currentTurn = info->currentTurn;

    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, cityId);
    if (!obj) {
        spdlog::error("Could not find city {:s}", idToString(cityId));
        return;
    }

    auto* fortification = static_cast<CFortification*>(obj);
    const std::string ownerKey = idToString(&fortification->ownerId);

    int& lastTurn = lastTurnByOwner[ownerKey];
    if (lastTurn == currentTurn) {
        spdlog::info("Transfer already used on turn {} for owner={}", currentTurn, ownerKey);
        return;
    }
    lastTurn = currentTurn;

    std::vector<CMidgardID> items;
    const int total = fortification->inventory.vftable->getItemsCount(&fortification->inventory);
    for (int i = 0; i < total; ++i) {
        if (auto id = fortification->inventory.vftable->getItem(&fortification->inventory, i)) {
            if (!filter || (*filter)(objectMap, id)) {
                items.push_back(*id);
            }
        }
    }
    if (items.empty()) {
        spdlog::info("No items in city {:s}", idToString(cityId));
        return;
    }

    auto fortOwner = getFortOwner(fortification, objectMap);
    auto* capital = findCapital(objectMap, fortOwner);
    if (!capital) {
        spdlog::warn("No capital found for owner={}", idToString(&fortification->ownerId));
        return;
    }

    CMidgardID capId = capital->id;
    transferItems(items, phaseGame, &capId, "cityStack", cityId, "city");
}

void __fastcall cityTransferBtn(game::CCityStackInterf* thisptr, int)
{
    transferCityToCapital(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId);
}


static void __fastcall cityInterfTestReverse(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    using namespace game;

    auto phaseGame = thisptr->dragDropInterf.phaseGame;
    if (!phaseGame || !phaseGame->data)
        return;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    if (!objectMap)
        return;

    // 1) реверс предметов в инвентаре города (без clear)
    auto cityObj = objectMap->vftable->findScenarioObjectById(objectMap,
                                                              &thisptr->data->fortificationId);
    if (!cityObj)
        return;

    auto fort = static_cast<CFortification*>(cityObj);
    auto& inv = fort->inventory;

    const int total = inv.vftable->getItemsCount(&inv);
    if (total <= 1)
        return;

    std::vector<CMidgardID> items;
    items.reserve(total);
    for (int i = 0; i < total; ++i) {
        if (auto id = inv.vftable->getItem(&inv, i)) {
            items.push_back(*id);
        }
    }

    // удаляем всё по id
    for (int i = 0; i < total; ++i) {
        const auto& id = items[i];
        inv.vftable->removeItem(&inv, /*a2=*/0, &id, objectMap);
    }
    // добавляем в обратном порядке
    for (int i = total - 1; i >= 0; --i) {
        const auto& id = items[i];
        inv.vftable->addItem(&inv, /*a2=*/0, &id, objectMap);
    }

    // 2) форс-обновление интерфейса через менеджер интерфейсов
    //    (скрыть и сразу показать тот же интерфейс)
    // manager можно получить из phaseGame->data->interfManager (как ты уже делал в
    // msgbox-хендлерах)
    auto managerImpl = phaseGame->data->interfManager.data; // CInterfManagerImpl*
    if (!managerImpl || !managerImpl->CInterfManagerImpl::CInterfManager::vftable)
        return;

    auto mgrVt = managerImpl->CInterfManagerImpl::CInterfManager::vftable;
    auto asIface = reinterpret_cast<CInterface*>(&thisptr->dragDropInterf); // базовый CInterface

    // hide -> show (мягкий рефреш без закрытия диалога)
    if (mgrVt->hideInterface && mgrVt->showInterface) {
        mgrVt->hideInterface(reinterpret_cast<CInterfManager*>(managerImpl), asIface);
        mgrVt->showInterface(reinterpret_cast<CInterfManager*>(managerImpl), asIface);
    }
}






void __fastcall cityInterfTransferAllToStack(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    transferCityToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId);
}

void __fastcall cityInterfTransferPotionsToStack(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    transferCityToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId,
                        isPotion);
}

void __fastcall cityInterfTransferSpellsToStack(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    transferCityToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId,
                        isSpell);
}

void __fastcall cityInterfTransferValuablesToStack(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    transferCityToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId,
                        isValuable);
}

static bool isItemEquipped(const game::IdVector& equippedItems, const game::CMidgardID* itemId)
{
    for (const game::CMidgardID* item = equippedItems.bgn; item != equippedItems.end; item++) {
        if (*item == *itemId) {
            return true;
        }
    }

    return false;
}

/** Transfers visiting stack items to city. */
static void transferStackToCity(game::CPhaseGame* phaseGame,
                                const game::CMidgardID* cityId,
                                std::optional<ItemFilter> itemFilter = std::nullopt)
{
    using namespace game;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    auto obj = objectMap->vftable->findScenarioObjectById(objectMap, cityId);
    if (!obj) {
        spdlog::error("Could not find city {:s}", idToString(cityId));
        return;
    }

    auto fortification = static_cast<CFortification*>(obj);
    if (fortification->stackId == emptyId) {
        return;
    }

    auto stackObj = objectMap->vftable->findScenarioObjectById(objectMap, &fortification->stackId);
    if (!stackObj) {
        spdlog::error("Could not find stack {:s}", idToString(&fortification->stackId));
        return;
    }

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto stack = (CMidStack*)dynamicCast(stackObj, 0, rtti.IMidScenarioObjectType,
                                         rtti.CMidStackType, 0);
    if (!stack) {
        spdlog::error("Failed to cast scenario oject {:s} to stack",
                      idToString(&fortification->stackId));
        return;
    }

    auto& inventory = stack->inventory;
    std::vector<CMidgardID> items;
    const int itemsTotal = inventory.vftable->getItemsCount(&inventory);
    for (int i = 0; i < itemsTotal; i++) {
        auto item = inventory.vftable->getItem(&inventory, i);
        if (isItemEquipped(stack->leaderEquippedItems, item)) {
            continue;
        }

        if (!itemFilter || (itemFilter && (*itemFilter)(objectMap, item))) {
            items.push_back(*item);
        }
    }

    transferItems(items, phaseGame, cityId, "city", &fortification->stackId, "stack");
}

void __fastcall cityInterfTransferAllToCity(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    transferStackToCity(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId);
}

void __fastcall cityInterfTransferPotionsToCity(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    transferStackToCity(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId,
                        isPotion);
}

void __fastcall cityInterfTransferSpellsToCity(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    transferStackToCity(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId,
                        isSpell);
}

void __fastcall cityInterfTransferValuablesToCity(game::CCityStackInterf* thisptr, int /*%edx*/)
{
    transferStackToCity(thisptr->dragDropInterf.phaseGame, &thisptr->data->fortificationId,
                        isValuable);
}

static void setupCityStackButtons(game::CCityStackInterf* thisptr, game::CDialogInterf* dialog)
{
    using namespace game;
    using CB = CCityStackInterfApi::Api::ButtonCallback;
    const auto& btn = CButtonInterfApi::get();
    const auto& api = CCityStackInterfApi::get();
    const auto& dlg = CDialogInterfApi::get();
    SmartPointer fun;
    auto free = SmartPointerApi::get().createOrFreeNoDtor;
    CB cb{};
    constexpr char name[] = "DLG_CITY_STACK";

    auto hook = [&](const char* ctrl, CB::Callback fn) {
        if (dlg.findControl(dialog, ctrl)) {
            cb.callback = fn;
            api.createButtonFunctor(&fun, 0, thisptr, &cb);
            btn.assignFunctor(dialog, ctrl, name, &fun, 0);
            free(&fun, nullptr);
        }
    };

    // --- TRANSFER кнопки ---
    hook("BTN_TRANSF_L_ALL", (CB::Callback)cityInterfTransferAllToStack);
    hook("BTN_TRANSF_R_ALL", (CB::Callback)cityInterfTransferAllToCity);
    hook("BTN_TRANSF_L_POTIONS", (CB::Callback)cityInterfTransferPotionsToStack);
    hook("BTN_TRANSF_R_POTIONS", (CB::Callback)cityInterfTransferPotionsToCity);
    hook("BTN_TRANSF_L_SPELLS", (CB::Callback)cityInterfTransferSpellsToStack);
    hook("BTN_TRANSF_R_SPELLS", (CB::Callback)cityInterfTransferSpellsToCity);
    hook("BTN_TRANSF_L_VALUABLES", (CB::Callback)cityInterfTransferValuablesToStack);
    hook("BTN_TRANSF_R_VALUABLES", (CB::Callback)cityInterfTransferValuablesToCity);
    hook("BTN_TRANSF_R_CITY_CAPITAL", (CB::Callback)cityTransferBtn);


    // --- SORT кнопки ---
    // 
    // --- SORT L --- Stack
   /* hook("BTN_CITY_SORT_L_ARMOR", (CB::Callback)cityInterfSort_L_Armor);
    hook("BTN_CITY_SORT_L_JEWEL", (CB::Callback)cityInterfSort_L_Jewel);
    hook("BTN_CITY_SORT_L_WEAPON", (CB::Callback)cityInterfSort_L_Weapon);
    hook("BTN_CITY_SORT_L_BANNER", (CB::Callback)cityInterfSort_L_Banner);
    hook("BTN_CITY_SORT_L_POTION_BOOST", (CB::Callback)cityInterfSort_L_PotionBoost);
    hook("BTN_CITY_SORT_L_POTION_HEAL", (CB::Callback)cityInterfSort_L_PotionHeal);
    hook("BTN_CITY_SORT_L_POTION_REVIVE", (CB::Callback)cityInterfSort_L_PotionRevive);
    hook("BTN_CITY_SORT_L_POTION_PERM", (CB::Callback)cityInterfSort_L_PotionPermanent);
    hook("BTN_CITY_SORT_L_SCROLL", (CB::Callback)cityInterfSort_L_Scroll);
    hook("BTN_CITY_SORT_L_WAND", (CB::Callback)cityInterfSort_L_Wand);
    hook("BTN_CITY_SORT_L_VALUABLE", (CB::Callback)cityInterfSort_L_Valuable);
    hook("BTN_CITY_SORT_L_ORB", (CB::Callback)cityInterfSort_L_Orb);
    hook("BTN_CITY_SORT_L_TALISMAN", (CB::Callback)cityInterfSort_L_Talisman);
    hook("BTN_CITY_SORT_L_TRAVEL", (CB::Callback)cityInterfSort_L_Travel);
    hook("BTN_CITY_SORT_L_SPECIAL", (CB::Callback)cityInterfSort_L_Special);*/

     // --- SORT R --- City
    hook("BTN_CITY_SORT_R_ARMOR", (CB::Callback)cityInterfSortArmor);
    hook("BTN_CITY_SORT_R_JEWEL", (CB::Callback)cityInterfSortJewel);
    hook("BTN_CITY_SORT_R_WEAPON", (CB::Callback)cityInterfSortWeapon);
    hook("BTN_CITY_SORT_R_BANNER", (CB::Callback)cityInterfSortBanner);
    hook("BTN_CITY_SORT_R_POTION_BOOST", (CB::Callback)cityInterfSortPotionBoost);
    hook("BTN_CITY_SORT_R_POTION_HEAL", (CB::Callback)cityInterfSortPotionHeal);
    hook("BTN_CITY_SORT_R_POTION_REVIVE", (CB::Callback)cityInterfSortPotionRevive);
    hook("BTN_CITY_SORT_R_POTION_PERMAMENT", (CB::Callback)cityInterfSortPotionPermanent);
    hook("BTN_CITY_SORT_R_SCROLL", (CB::Callback)cityInterfSortScroll);
    hook("BTN_CITY_SORT_R_WAND", (CB::Callback)cityInterfSortWand);
    hook("BTN_CITY_SORT_R_VALUABLE", (CB::Callback)cityInterfSortValuable);
    hook("BTN_CITY_SORT_R_ORB", (CB::Callback)cityInterfSortOrb);
    hook("BTN_CITY_SORT_R_TALISMAN", (CB::Callback)cityInterfSortTalisman);
    hook("BTN_CITY_SORT_R_TRAVEL", (CB::Callback)cityInterfSortTravel);
    hook("BTN_CITY_SORT_R_SPECIAL", (CB::Callback)cityInterfSortSpecial);
}

game::CCityStackInterf* __fastcall cityStackInterfCtorHooked(game::CCityStackInterf* thisptr,
                                                             int /*%edx*/,
                                                             void* taskOpenInterf,
                                                             game::CPhaseGame* phaseGame,
                                                             game::CMidgardID* cityId)
{
    using namespace game;

    getOriginalFunctions().cityStackInterfCtor(thisptr, taskOpenInterf, phaseGame, cityId);

    auto dialog = CDragAndDropInterfApi::get().getDialog(&thisptr->dragDropInterf);
    setupCityStackButtons(thisptr, dialog);

    return thisptr;
}



/** Transfers items from stack with srcStackId to stack with dstStackId. */
static void transferStackToStack(game::CPhaseGame* phaseGame,
                                 const game::CMidgardID* dstStackId,
                                 const game::CMidgardID* srcStackId,
                                 std::optional<ItemFilter> itemFilter = std::nullopt)
{
    using namespace game;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    auto srcObj = objectMap->vftable->findScenarioObjectById(objectMap, srcStackId);
    if (!srcObj) {
        spdlog::error("Could not find stack {:s}", idToString(srcStackId));
        return;
    }

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto srcStack = (CMidStack*)dynamicCast(srcObj, 0, rtti.IMidScenarioObjectType,
                                            rtti.CMidStackType, 0);
    if (!srcStack) {
        spdlog::error("Failed to cast scenario oject {:s} to stack", idToString(srcStackId));
        return;
    }

    auto& inventory = srcStack->inventory;
    std::vector<CMidgardID> items;
    const int itemsTotal = inventory.vftable->getItemsCount(&inventory);
    for (int i = 0; i < itemsTotal; i++) {
        auto item = inventory.vftable->getItem(&inventory, i);
        if (isItemEquipped(srcStack->leaderEquippedItems, item)) {
            continue;
        }

        if (!itemFilter || (itemFilter && (*itemFilter)(objectMap, item))) {
            items.push_back(*item);
        }
    }

    transferItems(items, phaseGame, dstStackId, "stack", srcStackId, "stack");
}

void __fastcall exchangeTransferAllToLeftStack(game::CExchangeInterf* thisptr, int /*%edx*/)
{
    transferStackToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackLeftSideId,
                         &thisptr->data->stackRightSideId);
}

void __fastcall exchangeTransferPotionsToLeftStack(game::CExchangeInterf* thisptr, int /*%edx*/)
{
    transferStackToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackLeftSideId,
                         &thisptr->data->stackRightSideId, isPotion);
}

void __fastcall exchangeTransferSpellsToLeftStack(game::CExchangeInterf* thisptr, int /*%edx*/)
{
    transferStackToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackLeftSideId,
                         &thisptr->data->stackRightSideId, isSpell);
}

void __fastcall exchangeTransferValuablesToLeftStack(game::CExchangeInterf* thisptr, int /*%edx*/)
{
    transferStackToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackLeftSideId,
                         &thisptr->data->stackRightSideId, isValuable);
}

void __fastcall exchangeTransferAllToRightStack(game::CExchangeInterf* thisptr, int /*%edx*/)
{
    transferStackToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackRightSideId,
                         &thisptr->data->stackLeftSideId);
}

void __fastcall exchangeTransferPotionsToRightStack(game::CExchangeInterf* thisptr, int /*%edx*/)
{
    transferStackToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackRightSideId,
                         &thisptr->data->stackLeftSideId, isPotion);
}

void __fastcall exchangeTransferSpellsToRightStack(game::CExchangeInterf* thisptr, int /*%edx*/)
{
    transferStackToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackRightSideId,
                         &thisptr->data->stackLeftSideId, isSpell);
}

void __fastcall exchangeTransferValuablesToRightStack(game::CExchangeInterf* thisptr, int /*%edx*/)
{
    transferStackToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackRightSideId,
                         &thisptr->data->stackLeftSideId, isValuable);
}

game::CExchangeInterf* __fastcall exchangeInterfCtorHooked(game::CExchangeInterf* thisptr,
                                                           int /*%edx*/,
                                                           void* taskOpenInterf,
                                                           game::CPhaseGame* phaseGame,
                                                           game::CMidgardID* stackLeftSide,
                                                           game::CMidgardID* stackRightSide)
{
    using namespace game;

    getOriginalFunctions().exchangeInterfCtor(thisptr, taskOpenInterf, phaseGame, stackLeftSide,
                                              stackRightSide);

    const auto& button = CButtonInterfApi::get();
    const auto freeFunctor = SmartPointerApi::get().createOrFreeNoDtor;
    const char dialogName[] = "DLG_EXCHANGE";
    auto dialog = CDragAndDropInterfApi::get().getDialog(&thisptr->dragDropInterf);

    using ButtonCallback = CExchangeInterfApi::Api::ButtonCallback;
    ButtonCallback callback{};
    SmartPointer functor;

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& exchangeInterf = CExchangeInterfApi::get();

    if (dialogApi.findControl(dialog, "BTN_TRANSF_L_ALL")) {
        callback.callback = (ButtonCallback::Callback)exchangeTransferAllToLeftStack;
        exchangeInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_L_ALL", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_R_ALL")) {
        callback.callback = (ButtonCallback::Callback)exchangeTransferAllToRightStack;
        exchangeInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_R_ALL", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_L_POTIONS")) {
        callback.callback = (ButtonCallback::Callback)exchangeTransferPotionsToLeftStack;
        exchangeInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_L_POTIONS", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_R_POTIONS")) {
        callback.callback = (ButtonCallback::Callback)exchangeTransferPotionsToRightStack;
        exchangeInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_R_POTIONS", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_L_SPELLS")) {
        callback.callback = (ButtonCallback::Callback)exchangeTransferSpellsToLeftStack;
        exchangeInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_L_SPELLS", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_R_SPELLS")) {
        callback.callback = (ButtonCallback::Callback)exchangeTransferSpellsToRightStack;
        exchangeInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_R_SPELLS", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_L_VALUABLES")) {
        callback.callback = (ButtonCallback::Callback)exchangeTransferValuablesToLeftStack;
        exchangeInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_L_VALUABLES", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_R_VALUABLES")) {
        callback.callback = (ButtonCallback::Callback)exchangeTransferValuablesToRightStack;
        exchangeInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_R_VALUABLES", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    return thisptr;
}

/** Transfers bag items to stack. */
static void transferBagToStack(game::CPhaseGame* phaseGame,
                               const game::CMidgardID* stackId,
                               const game::CMidgardID* bagId,
                               std::optional<ItemFilter> itemFilter = std::nullopt)
{
    using namespace game;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    auto bagObj = objectMap->vftable->findScenarioObjectById(objectMap, bagId);
    if (!bagObj) {
        spdlog::error("Could not find bag {:s}", idToString(bagId));
        return;
    }

    auto bag = static_cast<CMidBag*>(bagObj);
    auto& inventory = bag->inventory;
    std::vector<CMidgardID> items;
    const int itemsTotal = inventory.vftable->getItemsCount(&inventory);
    for (int i = 0; i < itemsTotal; i++) {
        auto item = inventory.vftable->getItem(&inventory, i);
        if (!itemFilter || (itemFilter && (*itemFilter)(objectMap, item))) {
            items.push_back(*item);
        }
    }

    transferItems(items, phaseGame, stackId, "stack", bagId, "bag");
}

void __fastcall pickupTransferAllToStack(game::CPickUpDropInterf* thisptr, int /*%edx*/)
{
    transferBagToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackId,
                       &thisptr->data->bagId);
}

void __fastcall pickupTransferPotionsToStack(game::CPickUpDropInterf* thisptr, int /*%edx*/)
{
    transferBagToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackId,
                       &thisptr->data->bagId, isPotion);
}

void __fastcall pickupTransferSpellsToStack(game::CPickUpDropInterf* thisptr, int /*%edx*/)
{
    transferBagToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackId,
                       &thisptr->data->bagId, isSpell);
}

void __fastcall pickupTransferValuablesToStack(game::CPickUpDropInterf* thisptr, int /*%edx*/)
{
    transferBagToStack(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackId,
                       &thisptr->data->bagId, isValuable);
}

/** Transfers stack items to bag. */
static void transferStackToBag(game::CPhaseGame* phaseGame,
                               const game::CMidgardID* stackId,
                               const game::CMidgardID* bagId,
                               std::optional<ItemFilter> itemFilter = std::nullopt)
{
    using namespace game;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    auto stackObj = objectMap->vftable->findScenarioObjectById(objectMap, stackId);
    if (!stackObj) {
        spdlog::error("Could not find stack {:s}", idToString(stackId));
        return;
    }

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto stack = (CMidStack*)dynamicCast(stackObj, 0, rtti.IMidScenarioObjectType,
                                         rtti.CMidStackType, 0);
    if (!stack) {
        spdlog::error("Failed to cast scenario oject {:s} to stack", idToString(stackId));
        return;
    }

    auto& inventory = stack->inventory;
    std::vector<CMidgardID> items;
    const int itemsTotal = inventory.vftable->getItemsCount(&inventory);
    for (int i = 0; i < itemsTotal; i++) {
        auto item = inventory.vftable->getItem(&inventory, i);
        if (isItemEquipped(stack->leaderEquippedItems, item)) {
            continue;
        }

        if (!itemFilter || (itemFilter && (*itemFilter)(objectMap, item))) {
            items.push_back(*item);
        }
    }

    transferItems(items, phaseGame, bagId, "bag", stackId, "stack");
}

void __fastcall pickupTransferAllToBag(game::CPickUpDropInterf* thisptr, int /*%edx*/)
{
    transferStackToBag(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackId,
                       &thisptr->data->bagId);
}

void __fastcall pickupTransferPotionsToBag(game::CPickUpDropInterf* thisptr, int /*%edx*/)
{
    transferStackToBag(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackId,
                       &thisptr->data->bagId, isPotion);
}

void __fastcall pickupTransferSpellsToBag(game::CPickUpDropInterf* thisptr, int /*%edx*/)
{
    transferStackToBag(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackId,
                       &thisptr->data->bagId, isSpell);
}

void __fastcall pickupTransferValuablesToBag(game::CPickUpDropInterf* thisptr, int /*%edx*/)
{
    transferStackToBag(thisptr->dragDropInterf.phaseGame, &thisptr->data->stackId,
                       &thisptr->data->bagId, isValuable);
}

game::CPickUpDropInterf* __fastcall pickupDropInterfCtorHooked(game::CPickUpDropInterf* thisptr,
                                                               int /*%edx*/,
                                                               void* taskOpenInterf,
                                                               game::CPhaseGame* phaseGame,
                                                               game::CMidgardID* stackId,
                                                               game::CMidgardID* bagId)
{
    using namespace game;

    getOriginalFunctions().pickupDropInterfCtor(thisptr, taskOpenInterf, phaseGame, stackId, bagId);

    const auto& button = CButtonInterfApi::get();
    const auto freeFunctor = SmartPointerApi::get().createOrFreeNoDtor;
    const char dialogName[] = "DLG_PICKUP_DROP";
    auto dialog = CDragAndDropInterfApi::get().getDialog(&thisptr->dragDropInterf);

    using ButtonCallback = CPickUpDropInterfApi::Api::ButtonCallback;
    ButtonCallback callback{};
    SmartPointer functor;

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& pickupInterf = CPickUpDropInterfApi::get();

    if (dialogApi.findControl(dialog, "BTN_TRANSF_L_ALL")) {
        callback.callback = (ButtonCallback::Callback)pickupTransferAllToStack;
        pickupInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_L_ALL", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_R_ALL")) {
        callback.callback = (ButtonCallback::Callback)pickupTransferAllToBag;
        pickupInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_R_ALL", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_L_POTIONS")) {
        callback.callback = (ButtonCallback::Callback)pickupTransferPotionsToStack;
        pickupInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_L_POTIONS", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_R_POTIONS")) {
        callback.callback = (ButtonCallback::Callback)pickupTransferPotionsToBag;
        pickupInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_R_POTIONS", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_L_SPELLS")) {
        callback.callback = (ButtonCallback::Callback)pickupTransferSpellsToStack;
        pickupInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_L_SPELLS", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_R_SPELLS")) {
        callback.callback = (ButtonCallback::Callback)pickupTransferSpellsToBag;
        pickupInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_R_SPELLS", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_L_VALUABLES")) {
        callback.callback = (ButtonCallback::Callback)pickupTransferValuablesToStack;
        pickupInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_L_VALUABLES", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_TRANSF_R_VALUABLES")) {
        callback.callback = (ButtonCallback::Callback)pickupTransferValuablesToBag;
        pickupInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_TRANSF_R_VALUABLES", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    return thisptr;
}

struct SellItemsMsgBoxHandler : public game::CMidMsgBoxButtonHandler
{
    game::CSiteMerchantInterf* merchantInterf;
};

void __fastcall sellItemsMsgBoxHandlerDtor(SellItemsMsgBoxHandler* thisptr,
                                           int /*%edx*/,
                                           char flags)
{
    if (flags & 1) {
        // We can skip a call to CMidMsgBoxButtonHandler destructor
        // since we know it does not have any members and does not free anything except its memory.
        // Free our memory here
        game::Memory::get().freeNonZero(thisptr);
    }
}

static void sellItemsToMerchant(game::CPhaseGame* phaseGame,
                                const game::CMidgardID* merchantId,
                                const game::CMidgardID* stackId,
                                std::optional<ItemFilter> itemFilter = std::nullopt)
{
    using namespace game;

    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    auto stackObj = objectMap->vftable->findScenarioObjectById(objectMap, stackId);
    if (!stackObj) {
        spdlog::error("Could not find stack {:s}", idToString(stackId));
        return;
    }

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto stack = (CMidStack*)dynamicCast(stackObj, 0, rtti.IMidScenarioObjectType,
                                         rtti.CMidStackType, 0);
    if (!stack) {
        spdlog::error("Failed to cast scenario oject {:s} to stack", idToString(stackId));
        return;
    }

    auto& inventory = stack->inventory;
    const int itemsTotal = inventory.vftable->getItemsCount(&inventory);

    std::vector<CMidgardID> itemsToSell;
    for (int i = 0; i < itemsTotal; ++i) {
        auto item = inventory.vftable->getItem(&inventory, i);
        if (!itemFilter || (itemFilter && (*itemFilter)(objectMap, item))) {
            itemsToSell.push_back(*item);
        }
    }

    const auto& sendSellItemMsg = NetMessagesApi::get().sendSiteSellItemMsg;
    for (const auto& item : itemsToSell) {
        sendSellItemMsg(phaseGame, merchantId, stackId, &item);
    }
}

void __fastcall sellValuablesMsgBoxHandlerFunction(SellItemsMsgBoxHandler* thisptr,
                                                   int /*%edx*/,
                                                   game::CMidgardMsgBox* msgBox,
                                                   bool okPressed)
{
    using namespace game;

    auto merchant = thisptr->merchantInterf;
    auto phaseGame = merchant->dragDropInterf.phaseGame;

    if (okPressed) {
        auto data = merchant->data;
        sellItemsToMerchant(phaseGame, &data->merchantId, &data->stackId, isValuable);
    }

    auto manager = phaseGame->data->interfManager.data;
    auto vftable = manager->CInterfManagerImpl::CInterfManager::vftable;
    vftable->hideInterface(manager, msgBox);

    if (msgBox) {
        msgBox->vftable->destructor(msgBox, 1);
    }
}

game::CMidMsgBoxButtonHandlerVftable sellValuablesMsgBoxHandlerVftable{
    (game::CMidMsgBoxButtonHandlerVftable::Destructor)sellItemsMsgBoxHandlerDtor,
    (game::CMidMsgBoxButtonHandlerVftable::Handler)sellValuablesMsgBoxHandlerFunction};

void __fastcall sellAllItemsMsgBoxHandlerFunction(SellItemsMsgBoxHandler* thisptr,
                                                  int /*%edx*/,
                                                  game::CMidgardMsgBox* msgBox,
                                                  bool okPressed)
{
    using namespace game;

    auto merchant = thisptr->merchantInterf;
    auto phaseGame = merchant->dragDropInterf.phaseGame;

    if (okPressed) {
        auto data = merchant->data;
        sellItemsToMerchant(phaseGame, &data->merchantId, &data->stackId);
    }

    auto manager = phaseGame->data->interfManager.data;
    auto vftable = manager->CInterfManagerImpl::CInterfManager::vftable;
    vftable->hideInterface(manager, msgBox);

    if (msgBox) {
        msgBox->vftable->destructor(msgBox, 1);
    }
}

game::CMidMsgBoxButtonHandlerVftable sellAllItemsMsgBoxHandlerVftable{
    (game::CMidMsgBoxButtonHandlerVftable::Destructor)sellItemsMsgBoxHandlerDtor,
    (game::CMidMsgBoxButtonHandlerVftable::Handler)sellAllItemsMsgBoxHandlerFunction};

static std::string bankToPriceMessage(const game::Bank& bank)
{
    using namespace game;

    std::string priceText;
    if (bank.gold) {
        auto text = getInterfaceText("X005TA0055");
        replace(text, "%QTY%", fmt::format("{:d}", bank.gold));
        priceText.append(text + '\n');
    }

    if (bank.runicMana) {
        auto text = getInterfaceText("X005TA0056");
        replace(text, "%QTY%", fmt::format("{:d}", bank.runicMana));
        priceText.append(text + '\n');
    }

    if (bank.deathMana) {
        auto text = getInterfaceText("X005TA0057");
        replace(text, "%QTY%", fmt::format("{:d}", bank.deathMana));
        priceText.append(text + '\n');
    }

    if (bank.lifeMana) {
        auto text = getInterfaceText("X005TA0058");
        replace(text, "%QTY%", fmt::format("{:d}", bank.lifeMana));
        priceText.append(text + '\n');
    }

    if (bank.infernalMana) {
        auto text = getInterfaceText("X005TA0059");
        replace(text, "%QTY%", fmt::format("{:d}", bank.infernalMana));
        priceText.append(text + '\n');
    }

    if (bank.groveMana) {
        auto text = getInterfaceText("X160TA0001");
        replace(text, "%QTY%", fmt::format("{:d}", bank.groveMana));
        priceText.append(text + '\n');
    }

    return priceText;
}

static game::Bank computeItemsSellPrice(game::IMidgardObjectMap* objectMap,
                                        const game::CMidgardID* stackId,
                                        std::optional<ItemFilter> itemFilter = std::nullopt)
{
    using namespace game;

    auto stackObj = objectMap->vftable->findScenarioObjectById(objectMap, stackId);
    if (!stackObj) {
        spdlog::error("Could not find stack {:s}", idToString(stackId));
        return {};
    }

    const auto dynamicCast = RttiApi::get().dynamicCast;
    const auto& rtti = RttiApi::rtti();

    auto stack = (CMidStack*)dynamicCast(stackObj, 0, rtti.IMidScenarioObjectType,
                                         rtti.CMidStackType, 0);
    if (!stack) {
        spdlog::error("Failed to cast scenario oject {:s} to stack", idToString(stackId));
        return {};
    }

    auto& inventory = stack->inventory;
    const int itemsTotal = inventory.vftable->getItemsCount(&inventory);
    auto getSellingPrice = CMidItemApi::get().getSellingPrice;
    auto bankAdd = BankApi::get().add;

    Bank sellPrice{};
    for (int i = 0; i < itemsTotal; ++i) {
        auto item = inventory.vftable->getItem(&inventory, i);
        if (!itemFilter || (itemFilter && (*itemFilter)(objectMap, item))) {
            Bank price{};
            getSellingPrice(&price, objectMap, item);
            bankAdd(&sellPrice, &price);
        }
    }

    return sellPrice;
}

void __fastcall merchantSellValuables(game::CSiteMerchantInterf* thisptr, int /*%edx*/)
{
    using namespace game;

    auto phaseGame = thisptr->dragDropInterf.phaseGame;
    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    auto stackId = &thisptr->data->stackId;

    const auto sellPrice = computeItemsSellPrice(objectMap, stackId, isValuable);
    if (BankApi::get().isZero(&sellPrice)) {
        // No items to sell
        return;
    }

    const auto priceText = bankToPriceMessage(sellPrice);
    auto message = getInterfaceText(textIds().interf.sellAllValuables.c_str());
    if (!message.empty()) {
        replace(message, "%PRICE%", priceText);
    } else {
        // Fallback in case of interface text could not be found
        message = fmt::format("Do you want to sell all valuables? Revenue will be:\n{:s}",
                              priceText);
    }

    auto memAlloc = Memory::get().allocate;
    auto handler = (SellItemsMsgBoxHandler*)memAlloc(sizeof(SellItemsMsgBoxHandler));
    handler->vftable = &sellValuablesMsgBoxHandlerVftable;
    handler->merchantInterf = thisptr;
    hooks::showMessageBox(message, handler, true);
}

void __fastcall merchantSellAll(game::CSiteMerchantInterf* thisptr, int /*%edx*/)
{
    using namespace game;

    auto phaseGame = thisptr->dragDropInterf.phaseGame;
    auto objectMap = CPhaseApi::get().getDataCache(&phaseGame->phase);
    auto stackId = &thisptr->data->stackId;

    const auto sellPrice = computeItemsSellPrice(objectMap, stackId);
    if (BankApi::get().isZero(&sellPrice)) {
        // No items to sell
        return;
    }

    const auto priceText = bankToPriceMessage(sellPrice);
    auto message = getInterfaceText(textIds().interf.sellAllItems.c_str());
    if (!message.empty()) {
        replace(message, "%PRICE%", priceText);
    } else {
        // Fallback in case of interface text could not be found
        message = fmt::format("Do you want to sell all items? Revenue will be:\n{:s}", priceText);
    }

    auto memAlloc = Memory::get().allocate;
    auto handler = (SellItemsMsgBoxHandler*)memAlloc(sizeof(SellItemsMsgBoxHandler));
    handler->vftable = &sellAllItemsMsgBoxHandlerVftable;
    handler->merchantInterf = thisptr;
    hooks::showMessageBox(message, handler, true);
}

game::CSiteMerchantInterf* __fastcall siteMerchantInterfCtorHooked(
    game::CSiteMerchantInterf* thisptr,
    int /*%edx*/,
    void* taskOpenInterf,
    game::CPhaseGame* phaseGame,
    game::CMidgardID* stackId,
    game::CMidgardID* merchantId)
{
    using namespace game;

    getOriginalFunctions().siteMerchantInterfCtor(thisptr, taskOpenInterf, phaseGame, stackId,
                                                  merchantId);

    const auto& button = CButtonInterfApi::get();
    const auto freeFunctor = SmartPointerApi::get().createOrFreeNoDtor;
    const char dialogName[] = "DLG_MERCHANT";
    auto dialog = CDragAndDropInterfApi::get().getDialog(&thisptr->dragDropInterf);

    using ButtonCallback = CSiteMerchantInterfApi::Api::ButtonCallback;
    ButtonCallback callback{};
    SmartPointer functor;

    const auto& dialogApi = CDialogInterfApi::get();
    const auto& merchantInterf = CSiteMerchantInterfApi::get();

    if (dialogApi.findControl(dialog, "BTN_SELL_ALL_VALUABLES")) {
        callback.callback = (ButtonCallback::Callback)merchantSellValuables;
        merchantInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_SELL_ALL_VALUABLES", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    if (dialogApi.findControl(dialog, "BTN_SELL_ALL")) {
        callback.callback = (ButtonCallback::Callback)merchantSellAll;
        merchantInterf.createButtonFunctor(&functor, 0, thisptr, &callback);
        button.assignFunctor(dialog, "BTN_SELL_ALL", dialogName, &functor, 0);
        freeFunctor(&functor, nullptr);
    }

    return thisptr;
}



} // namespace hooks
