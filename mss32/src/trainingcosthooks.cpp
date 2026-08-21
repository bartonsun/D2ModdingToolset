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

int lowerCostFromUnitModifiers(const game::CMidUnit* unit)
{
    using namespace game;

    if (!unit || !unit->unitImpl) {
        return 0;
    }

    const auto& idApi = CMidgardIDApi::get();
    const auto& rtti = RttiApi::rtti();
    const auto dynamicCast = RttiApi::get().dynamicCast;

    int sum = 0;
    int n = 0;
    CUmModifier* modifier = nullptr;
    for (auto curr = unit->unitImpl; curr; curr = modifier->data->prev) {
        modifier = (CUmModifier*)dynamicCast(curr, 0, rtti.IUsUnitType, rtti.CUmModifierType, 0);
        if (!modifier || !modifier->data) {
            break;
        }
        ++n;
        const CMidgardID modifierId = modifier->data->modifierId;
        char buf[16]{};
        idApi.toString(&modifierId, buf);
        bool added = false;
        CUmStack* stackUm = castUmModifierToUmStack(modifier);
        if (stackUm && stackUm->data && stackUm->data->lowerCost.initialized) {
            sum += stackUm->data->lowerCost.value;
            added = true;
        }
        if (!added && _stricmp(buf, "g000um7548") == 0) {
            sum += 25;
            added = true;
        }
        if (!added && _stricmp(buf, "g100um7548") == 0) {
            sum += 25;
            added = true;
        }
        if (!added && _stricmp(buf, "g070um0097") == 0) {
            sum += 8;
            added = true;
        }
        if (!added && _stricmp(buf, "g006um0068") == 0) {
            sum += 15;
            added = true;
        }
        if (!added && _stricmp(buf, "g070um0281") == 0) {
            sum += 15;
            added = true;
        }
        if (!added && _stricmp(buf, "g070um0282") == 0) {
            sum += 20;
            added = true;
        }
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
    CMidgardID luteItem{};
    idApi.fromString(&luteItem, "g000ig3022");

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
        if (midItem->globalItemId == luteItem) {
            sum += 25;
        }
    }
    return std::clamp(sum, 0, 100);
}

int lowerCostPercentForStack(const game::IMidgardObjectMap* objectMap,
                             const game::CMidgardID* stackId)
{
    if (!objectMap || !stackId) {
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
    const int ours = std::min(100, lowerCostFromUnitModifiers(leaderUnit)
                                       + lowerCostFromStackItems(objectMap, stack));
    return std::clamp(std::max(native, ours), 0, 100);
}

int lowerCostPercentForUnit(const game::IMidgardObjectMap* objectMap,
                            const game::CMidgardID* unitId)
{
    if (!objectMap || !unitId) {
        return 0;
    }

    const game::CMidgardID* stackId = game::gameFunctions().getStackIdByUnitId(objectMap, unitId);
    if (!stackId) {
        return 0;
    }

    return lowerCostPercentForStack(objectMap, stackId);
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

    if (!gameSettings().trainerCampLowerCost || g_scopeDepth <= 0 || g_discountApplied
        || g_lowerCostPercent <= 0 || !thisptr) {
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

    auto* data = thisptr->trainingCampData;
    const game::IMidgardObjectMap* objectMap = nullptr;
    if (data->phaseGame) {
        objectMap = game::CPhaseApi::get().getDataCache(&data->phaseGame->phase);
    }

    const int percent = lowerCostPercentForStack(objectMap, &data->stackId);
    spdlog::info("trainer ui percent={}", percent);
    TrainingDiscountScope scope{percent};
    getOriginalFunctions().trainUiAction(thisptr, a1, a2);
}

void __fastcall trainUiTextHooked(game::CSiteTrainingCampInterf* thisptr, int /*%edx*/)
{
    int percent = 0;
    if (gameSettings().trainerCampLowerCost && thisptr && thisptr->trainingCampData) {
        auto* data = thisptr->trainingCampData;
        const game::IMidgardObjectMap* objectMap = nullptr;
        if (data->phaseGame) {
            objectMap = game::CPhaseApi::get().getDataCache(&data->phaseGame->phase);
        }
        percent = lowerCostPercentForStack(objectMap, &data->stackId);
        spdlog::info("trainer ui percent={}", percent);
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

    int percent = lowerCostPercentForUnit(objectMap, a3);
    if (percent <= 0) {
        percent = lowerCostPercentForUnit(objectMap, a2);
    }
    if (percent <= 0) {
        percent = lowerCostPercentForStack(objectMap, a2);
    }

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

    int percent = lowerCostPercentForUnit(objectMap, a4);
    if (percent <= 0) {
        percent = lowerCostPercentForUnit(objectMap, a3);
    }
    if (percent <= 0) {
        percent = lowerCostPercentForStack(objectMap, a2);
    }

    TrainingDiscountScope scope{percent};
    return getOriginalFunctions().applyTrainAction(objectMap, a2, a3, a4, a5, a6);
}

} // namespace hooks
