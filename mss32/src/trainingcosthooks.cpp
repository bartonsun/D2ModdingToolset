#include "trainingcosthooks.h"
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

// 0x5009c8 is shared by CDDStackGroup, CDDStackNoActionGroup and CDDReinfGroup,
// so it fires for stack-group actions all over the interface, not just in the
// camp. Opening a discount scope there would put a leader's lowerCost on
// whatever Bank::Copy happens next. The camp always draws its own text first --
// in the client's log `trainer uiText enter` precedes every `uiAction` -- so the
// text hook, which is camp-only, records which group belongs to the camp. Only
// the pointer is compared; it is never dereferenced.
thread_local const void* g_campStackGroup = nullptr;

// The stack the camp is trained from. The group's own id1/id2 are not stack ids
// in every scenario -- measured 2026-09-01 in the client's log, where the text
// hook reports 25% from `trainingCampData->stackId` and the action hook, one
// second later on the same camp, reports 0 from those two ids. The id is kept
// rather than the percent so the fallback is recomputed at press time: the
// hero's state can change between drawing the text and pressing the slot.
thread_local game::CMidgardID g_campStackId = game::invalidId;
thread_local const game::CMidDragDropInterf* g_campDragDrop = nullptr;

static volatile unsigned long g_campUiAtMs = 0;
static thread_local int g_campPercent = 0;
static bool g_inPartyTrainingText = false;

} // namespace

bool trainerCampUiRecentlyActive()
{
    return g_campUiAtMs && GetTickCount() - g_campUiAtMs < 2500;
}

long trainerCampUiAgeMs()
{
    return g_campUiAtMs ? static_cast<long>(GetTickCount() - g_campUiAtMs) : -1;
}

void clearCampPriceWindow()
{
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

void applyLeaderLowerCostToBank(game::Bank* bank, int lowerCostPercent)
{
    if (!bank || lowerCostPercent <= 0) {
        return;
    }

    const int clamped = std::clamp(lowerCostPercent, 0, 100);
    const int factor = 100 - clamped;
    if (factor >= 100) {
        return;
    }

    // Direct per-resource math, not BankApi::multiply + divide: multiply
    // clamps every resource at 9999, so a 1124-gold price at factor 30 would
    // read 9999/100 = 99 instead of 337. Prices stay below 10000, so the
    // product fits an int and the result below the original value.
    const auto scale = [factor](std::int16_t value) {
        return static_cast<std::int16_t>(static_cast<int>(value) * factor / 100);
    };
    bank->gold = scale(bank->gold);
    bank->infernalMana = scale(bank->infernalMana);
    bank->lifeMana = scale(bank->lifeMana);
    bank->deathMana = scale(bank->deathMana);
    bank->runicMana = scale(bank->runicMana);
    bank->groveMana = scale(bank->groveMana);
}

// Barton, 2026-09-07: the camp discount must come from the mod's own function —
// «вы получаете размер скидки каким-то своим способом, а не используете функцию
// из luaApi». The leader's vtable slot is that function: custommodifier.cpp
// installs stackLeaderGetLowerCost there, which walks the whole Lua modifier
// chain (smnsAura group effects, the smns valueCap ceiling, per-thread script
// environments) and ends in the same number the merchant shop charges. A second
// source — a local table or a local ceiling — drifts from the mod the first
// time Barton edits either, which is exactly the fork he rejected.
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
    //
    // The amount comes with it because a scope that discounts the wrong copy
    // reads exactly like a scope that discounted the drawn price.
    spdlog::info("trainer bankCopy depth={} percent={} gold={}", g_scopeDepth,
                 g_lowerCostPercent, thisptr ? thisptr->gold : -1);
    return result;
}

// Observation point, not the discount point. Both train flows build their
// working price as a local copy through the Bank copy constructor --
// trainUnitAtTrainer at 0x5d9004, canAffordTrainCheck at 0x46d54f (Akella) --
// but the copy starts as a 1-gold template that a later Bank::Multiply turns
// into the charged price (client log 2026-09-07 11:53: the copy read gold=1
// while the dialog promised 79, and scaling it here to 0 made trainUnitAtTrainer
// charge nothing). The multiply return site in TrainingCostApi is where the
// discount belongs; this hook only records what the template carried.
game::Bank* __fastcall bankCopyCtorHooked(game::Bank* thisptr, int /*%edx*/,
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

// The discount point. Inside trainUnitAtTrainer the cost template is multiplied
// into the real charge by Bank::Multiply at 0x5d901f (Akella); the subtraction
// that charges the player reads that product. Scaling the product as the
// multiply returns charges exactly the figure the camp dialog shows.
game::Bank* __fastcall bankMultiplyHooked(game::Bank* thisptr, int /*%edx*/, std::int16_t value)
{
    game::Bank* result = getOriginalFunctions().bankMultiply(thisptr, value);

    if (!gameSettings().trainerCampLowerCost || g_scopeDepth <= 0 || g_lowerCostPercent <= 0
        || !thisptr) {
        return result;
    }

    const auto& trainApi = game::TrainingCostApi::get();
    const void* const ret = *static_cast<void**>(_AddressOfReturnAddress());
    if (!trainApi.multiplyReturnTrainUnit || ret != trainApi.multiplyReturnTrainUnit) {
        return result;
    }

    const std::int16_t before = thisptr->gold;
    applyLeaderLowerCostToBank(thisptr, g_lowerCostPercent);
    spdlog::info("trainer priceScale gold={}->{} percent={}", before, thisptr->gold,
                 g_lowerCostPercent);
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

void __fastcall trainUiActionHooked(game::CDDStackGroup* thisptr,
                                    int /*%edx*/,
                                    int a1,
                                    int a2)
{
    // 0x5009c8 is not a camp-interface method. In Discipl2.exe it sits at index
    // 17 of the CDDStackGroup / CDDStackNoActionGroup / CDDReinfGroup vftables,
    // its body reads [this+0x10] -> [data+4] -> getDataCache, and it ends in
    // `ret 8`. Reading it as CSiteTrainingCampInterf meant dereferencing offset
    // 0x24 of a 20-byte object -- the access violation in the client's log.
    if (!gameSettings().trainerCampLowerCost || !thisptr || !thisptr->data) {
        getOriginalFunctions().trainUiAction(thisptr, a1, a2);
        return;
    }

    // A pointer comparison cannot fault, so it comes first: every stack-group
    // action in the game reaches this slot and none of them should write a line
    // into the log the client sends us.
    if (thisptr != g_campStackGroup) {
        spdlog::debug("trainer uiAction skip=not-camp");
        getOriginalFunctions().trainUiAction(thisptr, a1, a2);
        return;
    }

    // Written before the lookups: a log that ends here names the hook the
    // process died in, instead of leaving two hooks that wrote the same line.
    spdlog::info("trainer uiAction enter");

    auto* data = thisptr->data;
    // The route the function itself takes: the disassembly of 0x5009c8 reads
    // [data+4] -> +8 -> getDataCache, i.e. exactly this. data->objectMap is only
    // a fallback -- a field offset nothing in this function confirms.
    const game::IMidgardObjectMap* objectMap = nullptr;
    if (data->phaseGame) {
        objectMap = game::CPhaseApi::get().getDataCache(&data->phaseGame->phase);
    }
    if (!objectMap) {
        objectMap = data->objectMap;
    }

    // Which of the group's two ids is the stack is not stated anywhere, so each
    // one is routed by its own type and an id that is neither returns 0.
    const char* source = "ids";
    int percent = lowerCostPercentForId(objectMap, &data->id1);
    if (percent <= 0) {
        percent = lowerCostPercentForId(objectMap, &data->id2);
    }
    // The visible price goes through this scope, so a group whose ids name no
    // stack has to reach the same hero the camp text already resolved --
    // otherwise the shown price stays full while the charged one is discounted.
    if (percent <= 0 && g_campStackId != game::invalidId) {
        percent = lowerCostPercentForId(objectMap, &g_campStackId);
        source = "camp";
    }

    spdlog::info("trainer ui percent={} from=action src={}", percent, source);
    // Reaching this line means the camp the text hook recorded is still open and
    // this is the unit just clicked in it. The dialog that follows reads both of
    // these, and the freshness window used to be stamped by the text hook alone
    // -- so a camp visit longer than the window went back to showing full price
    // while the charged price stayed discounted.
    g_campPercent = percent;
    g_campUiAtMs = GetTickCount();
    TrainingDiscountScope scope{percent};
    getOriginalFunctions().trainUiAction(thisptr, a1, a2);
}

void __fastcall trainUiTextHooked(game::CSiteTrainingCampInterf* thisptr, int /*%edx*/)
{
    // hooks.cpp only installs these hooks when the switch is on, so this guard
    // never fires in a shipped build. It stays because the hook body is the
    // only place that says what «off» means: every other hook here already
    // read the flag first, and this one read it three statements in, which is
    // the shape that breaks the day someone installs the hooks unconditionally.
    if (!gameSettings().trainerCampLowerCost) {
        getOriginalFunctions().setPartyTrainingText(thisptr);
        return;
    }

    spdlog::info("trainer uiText enter");

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
        // The action hook resolves this same id through the type-routed lookup, so a
        // log that shows the two disagreeing names the fallback as the broken half
        // instead of leaving the camp price unexplained.
        spdlog::info("trainer camp idcheck={}", lowerCostPercentForId(objectMap, &data->stackId));
    }
    TrainingDiscountScope scope{percent};
    g_campPercent = percent;
    g_inPartyTrainingText = true;
    getOriginalFunctions().setPartyTrainingText(thisptr);
    g_inPartyTrainingText = false;
    if (thisptr && thisptr->trainingCampData) {
        g_campStackGroup = thisptr->trainingCampData->stackGroup;
        g_campStackId = thisptr->trainingCampData->stackId;
        g_campDragDrop = thisptr;
    }
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
                                       int /*%edx*/,
                                       const char* value)
{
    if (!value || !*value || !gameSettings().trainerCampLowerCost || g_inPartyTrainingText
        || g_campPercent <= 0 || g_campStackId == game::invalidId) {
        return getOriginalFunctions().textBoxSetString(thisptr, value);
    }

    const char* ruGold = std::strstr(value, "\xE7\xEE\xEB\xEE\xF2");
    std::string lower;
    lower.reserve(std::strlen(value));
    for (const char* p = value; *p; ++p) {
        lower += (*p >= 'A' && *p <= 'Z') ? static_cast<char>(*p - 'A' + 'a') : *p;
    }
    const char* enGold = std::strstr(lower.c_str(), "gold");

    size_t goldIdx = static_cast<size_t>(-1);
    if (ruGold) {
        goldIdx = static_cast<size_t>(ruGold - value);
    }
    if (enGold) {
        const size_t idx = static_cast<size_t>(enGold - lower.c_str());
        if (goldIdx == static_cast<size_t>(-1) || idx < goldIdx) {
            goldIdx = idx;
        }
    }
    if (goldIdx == static_cast<size_t>(-1)) {
        return getOriginalFunctions().textBoxSetString(thisptr, value);
    }

    const char* firstRun = nullptr;
    size_t firstLen = 0;
    int runs = 0;
    for (const char* p = value; *p; ++p) {
        if (*p >= '0' && *p <= '9') {
            const char* start = p;
            while (*p >= '0' && *p <= '9') {
                ++p;
            }
            ++runs;
            if (runs == 1) {
                firstRun = start;
                firstLen = static_cast<size_t>(p - start);
            }
            --p;
        }
    }
    if (runs != 1 || firstLen == 0 || firstLen > 6
        || static_cast<size_t>(firstRun - value) >= goldIdx) {
        return getOriginalFunctions().textBoxSetString(thisptr, value);
    }

    int price = 0;
    for (size_t i = 0; i < firstLen; ++i) {
        price = price * 10 + (firstRun[i] - '0');
    }
    const int discounted = price * (100 - g_campPercent) / 100;
    if (price <= 0 || discounted <= 0 || discounted == price) {
        return getOriginalFunctions().textBoxSetString(thisptr, value);
    }

    std::string text{value, static_cast<size_t>(firstRun - value)};
    text += std::to_string(discounted);
    text += firstRun + firstLen;
    spdlog::info("trainer dialog price {} -> {}", price, discounted);
    return getOriginalFunctions().textBoxSetString(thisptr, text.c_str());
}

void __fastcall midDragDropInterfDtorHooked(game::CMidDragDropInterf* thisptr, int /*%edx*/)
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
