#include "trainingcosthooks.h"
#include "currency.h"
#include "dynamiccast.h"
#include "game.h"
#include "gameutils.h"
#include "itemutils.h"
#include "midgardid.h"
#include "miditem.h"
#include "midstack.h"
#include "midunit.h"
#include "modifierutils.h"
#include "originalfunctions.h"
#include "phase.h"
#include "phasegame.h"
#include "settings.h"
#include "sitetrainingcampinterf.h"
#include "trainingcostapi.h"
#include "ummodifier.h"
#include "umstack.h"
#include "usstackleader.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <spdlog/spdlog.h>

namespace hooks {
namespace {

thread_local int g_lowerCostPercent = 0;
thread_local int g_discountApplied = 0;
thread_local int g_scopeDepth = 0;

} // namespace

void applyLeaderLowerCostToBank(game::Bank* bank, int lowerCostPercent)
{
    if (!bank || lowerCostPercent <= 0) {
        return;
    }

    const int clamped = std::clamp(lowerCostPercent, 0, 100);
    const std::int16_t factor = static_cast<std::int16_t>(100 - clamped);
    if (factor >= 100) {
        return;
    }

    auto& bankApi = game::BankApi::get();
    bankApi.multiply(bank, factor);
    bankApi.divide(bank, 100);
}

int tablePercent(const game::TrainingDiscountData::Entry* table, int count, const char* id)
{
    for (int i = 0; i < count; ++i) {
        if (_stricmp(table[i].id, id) == 0) {
            return table[i].percent;
        }
    }
    return 0;
}

int lowerCostFromUnitModifiers(const game::CMidUnit* unit)
{
    using namespace game;

    if (!unit || !unit->unitImpl) {
        return 0;
    }

    const auto& idApi = CMidgardIDApi::get();
    const auto& rtti = RttiApi::rtti();
    const auto dynamicCast = RttiApi::get().dynamicCast;

    int tableCount = 0;
    const auto* table = TrainingDiscountData::modifiers(tableCount);

    int sum = 0;
    CUmModifier* modifier = nullptr;
    for (auto curr = unit->unitImpl; curr; curr = modifier->data->prev) {
        modifier = (CUmModifier*)dynamicCast(curr, 0, rtti.IUsUnitType, rtti.CUmModifierType, 0);
        if (!modifier || !modifier->data) {
            break;
        }
        CUmStack* stackUm = castUmModifierToUmStack(modifier);
        if (stackUm && stackUm->data && stackUm->data->lowerCost.initialized) {
            sum += stackUm->data->lowerCost.value;
            continue;
        }
        const CMidgardID modifierId = modifier->data->modifierId;
        char buf[16]{};
        idApi.toString(&modifierId, buf);
        sum += tablePercent(table, tableCount, buf);
    }
    return std::clamp(sum, 0, 100);
}

int lowerCostFromStackItems(const game::IMidgardObjectMap* objectMap,
                            const game::CMidStack* stack)
{
    using namespace game;

    if (!objectMap || !stack) {
        return 0;
    }

    const auto& idApi = CMidgardIDApi::get();

    int tableCount = 0;
    const auto* table = TrainingDiscountData::items(tableCount);

    const CMidInventory* inventory = &stack->inventory;
    const int count = inventory->vftable->getItemsCount(inventory);
    int sum = 0;
    for (int i = 0; i < count; ++i) {
        const CMidgardID* itemId = inventory->vftable->getItem(inventory, i);
        if (!itemId) {
            continue;
        }
        const auto* midItem = static_cast<const CMidItem*>(
            objectMap->vftable->findScenarioObjectById(objectMap, itemId));
        if (!midItem) {
            continue;
        }
        char buf[16]{};
        idApi.toString(&midItem->globalItemId, buf);
        sum += tablePercent(table, tableCount, buf);
    }
    return std::clamp(sum, 0, 100);
}

int lowerCostPercentForStack(const game::IMidgardObjectMap* objectMap,
                             const game::CMidgardID* stackId)
{
    if (!objectMap || !stackId) {
        return 0;
    }

    // The camp hooks receive raw arguments whose type the game never states. A
    // lookup that is handed something other than a stack must stop here rather
    // than inside Disciples' own code, which takes the id on trust.
    if (game::CMidgardIDApi::get().getType(stackId) != game::IdType::Stack) {
        return 0;
    }

    const game::CMidStack* stack = getStack(objectMap, stackId);
    if (!stack) {
        return 0;
    }

    const game::CMidUnit* leaderUnit = game::gameFunctions().findUnitById(objectMap,
                                                                          &stack->leaderId);
    if (!leaderUnit || !leaderUnit->unitImpl) {
        return 0;
    }

    const game::IUsStackLeader* leader = game::gameFunctions().castUnitImplToStackLeader(
        leaderUnit->unitImpl);
    int native = 0;
    if (leader) {
        native = leader->vftable->getLowerCost(leader);
    }
    if (native > 0) {
        return std::clamp(native, 0, 100);
    }
    const int fallback = lowerCostFromUnitModifiers(leaderUnit)
                         + lowerCostFromStackItems(objectMap, stack);
    return std::clamp(fallback, 0, 100);
}

int lowerCostPercentForUnit(const game::IMidgardObjectMap* objectMap,
                            const game::CMidgardID* unitId)
{
    if (!objectMap || !unitId) {
        return 0;
    }

    if (game::CMidgardIDApi::get().getType(unitId) != game::IdType::Unit) {
        return 0;
    }

    const game::CMidgardID* stackId = game::gameFunctions().getStackIdByUnitId(objectMap, unitId);
    if (!stackId) {
        return 0;
    }

    return lowerCostPercentForStack(objectMap, stackId);
}

int lowerCostPercentForId(const game::IMidgardObjectMap* objectMap,
                          const game::CMidgardID* id)
{
    if (!objectMap || !id) {
        return 0;
    }

    switch (game::CMidgardIDApi::get().getType(id)) {
    case game::IdType::Unit:
        return lowerCostPercentForUnit(objectMap, id);
    case game::IdType::Stack:
        return lowerCostPercentForStack(objectMap, id);
    default:
        return 0;
    }
}

TrainingDiscountScope::TrainingDiscountScope(int lowerCostPercent)
{
    if (g_scopeDepth == 0) {
        g_lowerCostPercent = lowerCostPercent;
        g_discountApplied = 0;
    }
    ++g_scopeDepth;
}

TrainingDiscountScope::~TrainingDiscountScope()
{
    --g_scopeDepth;
    if (g_scopeDepth == 0) {
        g_lowerCostPercent = 0;
        g_discountApplied = 0;
    }
}

game::Bank* __fastcall bankCopyHooked(game::Bank* thisptr, int /*%edx*/, const game::Bank* other)
{
    game::Bank* result = getOriginalFunctions().bankCopy(thisptr, other);

    if (!gameSettings().trainerCampLowerCost || g_scopeDepth <= 0) {
        return result;
    }

    // Every Bank::Copy that happens while a training scope is open, applied or
    // not. Without this line a scope that the game never routes through
    // Bank::Copy is indistinguishable from one where the discount was already
    // spent, and both look like silence in the log.
    spdlog::info("trainer bankCopy depth={} percent={} applied={}", g_scopeDepth,
                 g_lowerCostPercent, g_discountApplied);

    if (g_discountApplied || g_lowerCostPercent <= 0 || !thisptr) {
        return result;
    }

    applyLeaderLowerCostToBank(thisptr, g_lowerCostPercent);
    g_discountApplied = 1;
    spdlog::info("trainer lowerCost apply percent={}", g_lowerCostPercent);
    return result;
}

bool __stdcall trainUnitAtTrainerHooked(game::IMidgardObjectMap* objectMap,
                                        const game::CMidgardID* playerId,
                                        const game::CMidgardID* unitId,
                                        int apply)
{
    if (!gameSettings().trainerCampLowerCost) {
        return getOriginalFunctions().trainUnitAtTrainer(objectMap, playerId, unitId, apply);
    }

    spdlog::info("trainer trainUnit enter");

    const int percent = lowerCostPercentForUnit(objectMap, unitId);
    spdlog::info("trainer trainUnit percent={}", percent);
    TrainingDiscountScope scope{percent};
    return getOriginalFunctions().trainUnitAtTrainer(objectMap, playerId, unitId, apply);
}

void __fastcall trainUiActionHooked(game::CSiteTrainingCampInterf* thisptr,
                                    int /*%edx*/,
                                    int a1,
                                    int a2)
{
    if (!gameSettings().trainerCampLowerCost || !thisptr || !thisptr->trainingCampData) {
        getOriginalFunctions().trainUiAction(thisptr, a1, a2);
        return;
    }

    // Written before the lookup: a log that ends here names the hook the process
    // died in, instead of leaving two hooks that wrote the same line.
    spdlog::info("trainer uiAction enter");

    auto* data = thisptr->trainingCampData;
    const game::IMidgardObjectMap* objectMap = nullptr;
    if (data->phaseGame) {
        objectMap = game::CPhaseApi::get().getDataCache(&data->phaseGame->phase);
    }

    const int percent = lowerCostPercentForStack(objectMap, &data->stackId);
    spdlog::info("trainer ui percent={} from=action", percent);
    TrainingDiscountScope scope{percent};
    getOriginalFunctions().trainUiAction(thisptr, a1, a2);
}

void __fastcall trainUiTextHooked(game::CSiteTrainingCampInterf* thisptr, int /*%edx*/)
{
    spdlog::info("trainer uiText enter");

    int percent = 0;
    if (gameSettings().trainerCampLowerCost && thisptr && thisptr->trainingCampData) {
        auto* data = thisptr->trainingCampData;
        const game::IMidgardObjectMap* objectMap = nullptr;
        if (data->phaseGame) {
            objectMap = game::CPhaseApi::get().getDataCache(&data->phaseGame->phase);
        }
        percent = lowerCostPercentForStack(objectMap, &data->stackId);
        spdlog::info("trainer ui percent={} from=text", percent);
    }
    TrainingDiscountScope scope{percent};
    getOriginalFunctions().setPartyTrainingText(thisptr);
}

bool __stdcall canAffordTrainCheckHooked(game::IMidgardObjectMap* objectMap,
                                         const game::CMidgardID* a2,
                                         const game::CMidgardID* a3)
{
    if (!gameSettings().trainerCampLowerCost) {
        return getOriginalFunctions().canAffordTrainCheck(objectMap, a2, a3);
    }

    spdlog::info("trainer canAfford enter");

    int percent = lowerCostPercentForId(objectMap, a3);
    if (percent <= 0) {
        percent = lowerCostPercentForId(objectMap, a2);
    }

    spdlog::info("trainer canAfford percent={}", percent);
    TrainingDiscountScope scope{percent};
    return getOriginalFunctions().canAffordTrainCheck(objectMap, a2, a3);
}

bool __stdcall applyTrainActionHooked(game::IMidgardObjectMap* objectMap,
                                      const game::CMidgardID* a2,
                                      const game::CMidgardID* a3,
                                      const game::CMidgardID* a4,
                                      int a5,
                                      int a6)
{
    if (!gameSettings().trainerCampLowerCost) {
        return getOriginalFunctions().applyTrainAction(objectMap, a2, a3, a4, a5, a6);
    }

    spdlog::info("trainer applyTrain enter");

    int percent = lowerCostPercentForId(objectMap, a4);
    if (percent <= 0) {
        percent = lowerCostPercentForId(objectMap, a3);
    }
    if (percent <= 0) {
        percent = lowerCostPercentForId(objectMap, a2);
    }

    spdlog::info("trainer applyTrain percent={}", percent);
    TrainingDiscountScope scope{percent};
    return getOriginalFunctions().applyTrainAction(objectMap, a2, a3, a4, a5, a6);
}

} // namespace hooks
