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

    using AddExperience = bool(__stdcall*)(const CMidgardID* unitId,
                                           int experience,
                                           IMidgardObjectMap* objectMap,
                                           int a4);

    TrainUnitAtTrainer trainUnitAtTrainer;
    TrainUiAction trainUiAction;
    CanAffordTrainCheck canAffordTrainCheck;
    ApplyTrainAction applyTrainAction;

        AddExperience addExperience;

        const void* costCopyReturnTrainUnit;
    const void* costCopyReturnCanAfford;

        const void* expReturnTrainUnit;

    const void* countCopyReturnTrainable;
    const void* countStepReturnTrainable;
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
