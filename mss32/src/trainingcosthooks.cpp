#include "trainingcosthooks.h"
#include "trainerlabeltext.h"
#include "currency.h"
#include "ddstackgroup.h"
#include "game.h"
#include "gameutils.h"
#include "midgardid.h"
#include "midstack.h"
#include "midunit.h"
#include "originalfunctions.h"
#include "phase.h"
#include "phasegame.h"
#include "settings.h"
#include "sitetrainingcampinterf.h"
#include "trainingcostapi.h"
#include "unitutils.h"
#include "usstackleader.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <spdlog/spdlog.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <intrin.h>

namespace hooks {
namespace {

thread_local int g_lowerCostPercent = 0;
thread_local int g_scopeDepth = 0;

thread_local const void* g_campStackGroup = nullptr;

thread_local game::CMidgardID g_campStackId = game::invalidId;
thread_local const game::CMidDragDropInterf* g_campDragDrop = nullptr;

static volatile unsigned long g_campUiAtMs = 0;
static thread_local int g_campPercent = 0;

} // namespace

bool trainerCampUiRecentlyActive()
{
    return g_campUiAtMs && GetTickCount() - g_campUiAtMs < 2500;
}

long trainerCampUiAgeMs()
{
    return g_campUiAtMs ? static_cast<long>(GetTickCount() - g_campUiAtMs) : -1;
}

void resetDiscountScope()
{
    g_scopeDepth = 0;
    g_lowerCostPercent = 0;
}

void clearCampPriceWindow()
{
    resetDiscountScope();
    g_campUiAtMs = 0;
    g_campPercent = 0;
    g_campStackId = game::invalidId;
    g_campStackGroup = nullptr;
    g_campDragDrop = nullptr;
}

bool trainerCampSessionOpen()
{
    return g_campStackId != game::invalidId;
}

int discountedGold(int gold, int lowerCostPercent)
{
    const int clamped = std::clamp(lowerCostPercent, 1, 99);
    const long long discounted = (static_cast<long long>(gold) * (100 - clamped)) / 100;
    return static_cast<int>(std::max<long long>(discounted, 1));
}

int lowerCostPercentForStack(const game::IMidgardObjectMap* objectMap,
                             const game::CMidgardID* stackId)
{
    if (!objectMap || !stackId) {
        return 0;
    }

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
    if (!leader) {
        return 0;
    }

    return std::clamp(leader->vftable->getLowerCost(leader), 0, 100);
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
    }
    ++g_scopeDepth;
}

TrainingDiscountScope::~TrainingDiscountScope()
{
    --g_scopeDepth;
    if (g_scopeDepth == 0) {
        g_lowerCostPercent = 0;
    }
}

game::Bank* __fastcall bankCopyHooked(game::Bank* thisptr, int , const game::Bank* other)
{
    game::Bank* result = getOriginalFunctions().bankCopy(thisptr, other);

    if (!gameSettings().trainerCampLowerCost || g_scopeDepth <= 0) {
        return result;
    }

    spdlog::info("trainer bankCopy depth={} percent={} gold={} ret={:#x}", g_scopeDepth,
                 g_lowerCostPercent, thisptr ? thisptr->gold : -1,
                 reinterpret_cast<std::uintptr_t>(*static_cast<void**>(_AddressOfReturnAddress())));
    return result;
}

game::Bank* __fastcall bankCopyCtorHooked(game::Bank* thisptr, int ,
                                          const game::Bank* other)
{
    game::Bank* result = getOriginalFunctions().bankCopyCtor(thisptr, other);

    if (!gameSettings().trainerCampLowerCost || g_scopeDepth <= 0 || !thisptr) {
        return result;
    }

    const auto& trainApi = game::TrainingCostApi::get();
    const void* const ret = *static_cast<void**>(_AddressOfReturnAddress());
    const char* site = nullptr;
    if (trainApi.costCopyReturnTrainUnit && ret == trainApi.costCopyReturnTrainUnit) {
        site = "train";
    } else if (trainApi.costCopyReturnCanAfford && ret == trainApi.costCopyReturnCanAfford) {
        site = "afford";
    } else {
        return result;
    }

    spdlog::info("trainer costCopy site={} gold={}", site, thisptr->gold);
    return result;
}

game::Bank* __fastcall bankSubtractHooked(game::Bank* thisptr, int ,
                                         const game::Bank* other)
{
    if (!gameSettings().trainerCampLowerCost || !thisptr || g_scopeDepth <= 0
        || g_lowerCostPercent <= 0) {
        return getOriginalFunctions().bankSubtract(thisptr, other);
    }

    game::Bank before = *thisptr;

    game::Bank* result = getOriginalFunctions().bankSubtract(thisptr, other);

    const int percent = std::clamp(g_lowerCostPercent, 1, 99);
    const auto& bankApi = game::BankApi::get();
    int paidGold = 0;
    int refundedGold = 0;
    int refundedTotal = 0;
    for (int t = 0; t < 6; ++t) {
        const auto type = static_cast<game::CurrencyType>(t);
        const int paid = bankApi.get(&before, type) - bankApi.get(thisptr, type);
        if (paid <= 0) {
            continue;
        }
        const int keep = std::max(paid * (100 - percent) / 100, 1);
        const int refund = paid - keep;
        if (refund <= 0) {
            continue;
        }
        const int refunded = std::min(bankApi.get(thisptr, type) + refund, 9999);
        bankApi.set(thisptr, type, static_cast<std::int16_t>(refunded));
        refundedTotal += refund;
        if (type == game::CurrencyType::Gold) {
            paidGold = paid;
            refundedGold = refund;
        }
    }
    if (refundedTotal > 0) {
        spdlog::info("trainer priceRefund paid={} refund={} percent={}", paidGold,
                     refundedGold, percent);
    }
    return result;
}

bool __stdcall addExperienceHooked(game::CMidgardID* unitId,
                                   int experience,
                                   game::IMidgardObjectMap* objectMap,
                                   int a4)
{
    if (!gameSettings().trainerCampLowerCost || g_scopeDepth <= 0 || g_lowerCostPercent <= 0) {
        return getOriginalFunctions().addExperience(unitId, experience, objectMap, a4);
    }

    spdlog::info("trainer exp vanilla {} percent={}", experience, g_lowerCostPercent);
    return getOriginalFunctions().addExperience(unitId, experience, objectMap, a4);
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

void __fastcall trainUiActionHooked(game::CDDStackGroup* thisptr,
                                    int ,
                                    int a1,
                                    int a2)
{
    if (!gameSettings().trainerCampLowerCost || !thisptr || !thisptr->data) {
        getOriginalFunctions().trainUiAction(thisptr, a1, a2);
        return;
    }

    if (thisptr != g_campStackGroup) {
        spdlog::debug("trainer uiAction skip=not-camp");
        getOriginalFunctions().trainUiAction(thisptr, a1, a2);
        return;
    }

    spdlog::info("trainer uiAction enter");

    auto* data = thisptr->data;
    const game::IMidgardObjectMap* objectMap = nullptr;
    if (data->phaseGame) {
        objectMap = game::CPhaseApi::get().getDataCache(&data->phaseGame->phase);
    }
    if (!objectMap) {
        objectMap = data->objectMap;
    }

    const char* source = "ids";
    int percent = lowerCostPercentForId(objectMap, &data->id1);
    if (percent <= 0) {
        percent = lowerCostPercentForId(objectMap, &data->id2);
    }
    if (percent <= 0 && g_campStackId != game::invalidId) {
        percent = lowerCostPercentForId(objectMap, &g_campStackId);
        source = "camp";
    }

    spdlog::info("trainer ui percent={} from=action src={}", percent, source);
    g_campPercent = percent;
    g_campUiAtMs = GetTickCount();
    TrainingDiscountScope scope{percent};
    getOriginalFunctions().trainUiAction(thisptr, a1, a2);
}

void __fastcall trainUiTextHooked(game::CSiteTrainingCampInterf* thisptr, int )
{
    if (!gameSettings().trainerCampLowerCost) {
        getOriginalFunctions().setPartyTrainingText(thisptr);
        return;
    }

    spdlog::info("trainer uiText enter");

    resetDiscountScope();
    g_campUiAtMs = GetTickCount();

    g_campStackGroup = nullptr;
    g_campStackId = game::invalidId;
    int percent = 0;
    if (thisptr && thisptr->trainingCampData) {
        auto* data = thisptr->trainingCampData;
        const game::IMidgardObjectMap* objectMap = nullptr;
        if (data->phaseGame) {
            objectMap = game::CPhaseApi::get().getDataCache(&data->phaseGame->phase);
        }
        percent = lowerCostPercentForStack(objectMap, &data->stackId);
        spdlog::info("trainer ui percent={} from=text", percent);
        spdlog::info("trainer camp idcheck={}", lowerCostPercentForId(objectMap, &data->stackId));
    }
    TrainingDiscountScope scope{percent};
    g_campPercent = percent;
    if (thisptr && thisptr->trainingCampData) {
        g_campStackGroup = thisptr->trainingCampData->stackGroup;
        g_campStackId = thisptr->trainingCampData->stackId;
        g_campDragDrop = thisptr;
    }
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

void __fastcall textBoxSetStringHooked(game::CTextBoxInterf* thisptr,
                                       int ,
                                       const char* value)
{
    if (!value || !*value || !gameSettings().trainerCampLowerCost || g_campPercent <= 0
        || g_campStackId == game::invalidId) {
        return getOriginalFunctions().textBoxSetString(thisptr, value);
    }

    static thread_local int g_textDiag = 0;
    if (g_textDiag < 40) {
        ++g_textDiag;
        const size_t len = std::strlen(value);
        spdlog::info("trainer text[{}] {}", g_textDiag,
                     std::string{value, value + std::min<size_t>(len, 160)});
    }

    const char* ruGold = std::strstr(value, "\xE7\xEE\xEB\xEE\xF2\xFB\xF5");
    if (!ruGold) {
        return getOriginalFunctions().textBoxSetString(thisptr, value);
    }

    const auto goldRun = TrainerLabelText::goldRunBefore(value, ruGold);
    if (goldRun.len == 0) {
        return getOriginalFunctions().textBoxSetString(thisptr, value);
    }

    int gold = 0;
    for (size_t i = 0; i < goldRun.len; ++i) {
        gold = gold * 10 + (goldRun.begin[i] - '0');
    }

    const int discounted = discountedGold(gold, g_campPercent);
    if (discounted >= gold) {
        return getOriginalFunctions().textBoxSetString(thisptr, value);
    }

    std::string text{value, static_cast<size_t>(goldRun.begin - value)};
    text += std::to_string(discounted);
    text += goldRun.begin + goldRun.len;
    spdlog::info("trainer dialog price {} -> {} percent={}", gold, discounted, g_campPercent);
    return getOriginalFunctions().textBoxSetString(thisptr, text.c_str());
}

void __fastcall midDragDropInterfDtorHooked(game::CMidDragDropInterf* thisptr, int )
{
    if (!gameSettings().trainerCampLowerCost) {
        getOriginalFunctions().midDragDropInterfDtor(thisptr);
        return;
    }
    if (thisptr == g_campDragDrop) {
        clearCampPriceWindow();
    }
    getOriginalFunctions().midDragDropInterfDtor(thisptr);
}

} // namespace hooks
