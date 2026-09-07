#ifndef TRAININGCOSTAPI_H
#define TRAININGCOSTAPI_H

#include "currency.h"
#include "midgardid.h"

namespace game {

struct IMidgardObjectMap;
struct CSiteTrainingCampInterf;
struct CDDStackGroup;

namespace TrainingCostApi {

struct Api
{
    using TrainUnitAtTrainer = bool(__stdcall*)(IMidgardObjectMap* objectMap,
                                                const CMidgardID* playerId,
                                                const CMidgardID* unitId,
                                                int apply);

    using TrainUiAction = void(__thiscall*)(CDDStackGroup* thisptr, int a1, int a2);

    using CanAffordTrainCheck = bool(__stdcall*)(IMidgardObjectMap* objectMap,
                                                 const CMidgardID* a2,
                                                 const CMidgardID* a3);

    using ApplyTrainAction = bool(__stdcall*)(IMidgardObjectMap* objectMap,
                                              const CMidgardID* a2,
                                              const CMidgardID* a3,
                                              const CMidgardID* a4,
                                              int a5,
                                              int a6);

    TrainUnitAtTrainer trainUnitAtTrainer;
    TrainUiAction trainUiAction;
    CanAffordTrainCheck canAffordTrainCheck;
    ApplyTrainAction applyTrainAction;

    /**
     * Return addresses seen by the Bank copy-constructor when the two train
     * flows build their local copy of IUsSoldier::getTrainingCost. The copy
     * starts as a 1-gold template (client log 2026-09-07 11:53: the copy read
     * gold=1 while the dialog promised 79), so scaling it at these sites
     * zeroes the template and the later multiply charges nothing -- they are
     * observation-only now. GoG values are the measured Akella sites shifted
     * by the same function pair deltas as the four hook addresses above.
     */
    const void* costCopyReturnTrainUnit;
    const void* costCopyReturnCanAfford;

    /**
     * Return address of the Bank::Multiply call that turns the 1-gold cost
     * template into the charged price (Akella: multiply at 0x5d901f inside
     * trainUnitAtTrainer, returns to 0x5d9024). The discount scales the bank
     * when this multiply returns, so the subtraction that follows charges
     * exactly the discounted figure the dialog shows. Derived for GoG from
     * the cost-copy return inside the same function (+0x1b), same
     * caveat as the rows above.
     */
    const void* multiplyReturnTrainUnit;
};

Api& get();

} // namespace TrainingCostApi

namespace TrainCampTextApi {

struct Api
{
    using SetPartyTrainingText = void(__thiscall*)(CSiteTrainingCampInterf* thisptr);
    SetPartyTrainingText setPartyTrainingText;
};

Api& get();

} // namespace TrainCampTextApi

} // namespace game

#endif // TRAININGCOSTAPI_H
