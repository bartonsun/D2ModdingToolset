#ifndef TRAININGCOSTHOOKS_H
#define TRAININGCOSTHOOKS_H

#include "currency.h"
#include "midgardid.h"
#include "sitetrainingcampinterf.h"

namespace game {

struct IMidgardObjectMap;
struct CSiteTrainingCampInterf;

} // namespace game

namespace hooks {

void applyLeaderLowerCostToBank(game::Bank* bank, int lowerCostPercent);

int lowerCostPercentForStack(const game::IMidgardObjectMap* objectMap,
                             const game::CMidgardID* stackId);

int lowerCostPercentForUnit(const game::IMidgardObjectMap* objectMap,
                            const game::CMidgardID* unitId);

struct TrainingDiscountScope
{
    explicit TrainingDiscountScope(int lowerCostPercent);
    ~TrainingDiscountScope();

    TrainingDiscountScope(const TrainingDiscountScope&) = delete;
    TrainingDiscountScope& operator=(const TrainingDiscountScope&) = delete;
};

game::Bank* __fastcall bankCopyHooked(game::Bank* thisptr, int /*%edx*/, const game::Bank* other);

bool __stdcall trainUnitAtTrainerHooked(game::IMidgardObjectMap* objectMap,
                                        const game::CMidgardID* playerId,
                                        const game::CMidgardID* unitId,
                                        int apply);

void __fastcall trainUiActionHooked(game::CSiteTrainingCampInterf* thisptr,
                                    int /*%edx*/,
                                    int a1,
                                    int a2);

void __fastcall trainUiTextHooked(game::CSiteTrainingCampInterf* thisptr, int /*%edx*/);

bool __stdcall canAffordTrainCheckHooked(game::IMidgardObjectMap* objectMap,
                                         const game::CMidgardID* a2,
                                         const game::CMidgardID* a3);

bool __stdcall applyTrainActionHooked(game::IMidgardObjectMap* objectMap,
                                      const game::CMidgardID* a2,
                                      const game::CMidgardID* a3,
                                      const game::CMidgardID* a4,
                                      int a5,
                                      int a6);

} // namespace hooks

#endif // TRAININGCOSTHOOKS_H
