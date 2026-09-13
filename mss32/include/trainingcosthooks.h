#ifndef TRAININGCOSTHOOKS_H
#define TRAININGCOSTHOOKS_H

#include "currency.h"
#include "midgardid.h"
#include "sitetrainingcampinterf.h"

namespace game {

struct IMidgardObjectMap;
struct CSiteTrainingCampInterf;
struct CDDStackGroup;
struct CTextBoxInterf;

} // namespace game

namespace hooks {

int boostedExperience(int experience, int lowerCostPercent);

int lowerCostPercentForStack(const game::IMidgardObjectMap* objectMap,
                             const game::CMidgardID* stackId);

int lowerCostPercentForUnit(const game::IMidgardObjectMap* objectMap,
                            const game::CMidgardID* unitId);

int lowerCostPercentForId(const game::IMidgardObjectMap* objectMap, const game::CMidgardID* id);

struct TrainingDiscountScope
{
    explicit TrainingDiscountScope(int lowerCostPercent);
    ~TrainingDiscountScope();

    TrainingDiscountScope(const TrainingDiscountScope&) = delete;
    TrainingDiscountScope& operator=(const TrainingDiscountScope&) = delete;
};

game::Bank* __fastcall bankCopyHooked(game::Bank* thisptr, int , const game::Bank* other);

game::Bank* __fastcall bankCopyCtorHooked(game::Bank* thisptr, int ,
                                          const game::Bank* other);

bool __stdcall addExperienceHooked(game::CMidgardID* unitId,
                                   int experience,
                                   game::IMidgardObjectMap* objectMap,
                                   int a4);

bool __stdcall trainUnitAtTrainerHooked(game::IMidgardObjectMap* objectMap,
                                        const game::CMidgardID* playerId,
                                        const game::CMidgardID* unitId,
                                        int apply);

void __fastcall trainUiActionHooked(game::CDDStackGroup* thisptr,
                                    int ,
                                    int a1,
                                    int a2);

void __fastcall trainUiTextHooked(game::CSiteTrainingCampInterf* thisptr, int );

void __fastcall textBoxSetStringHooked(game::CTextBoxInterf* thisptr,
                                       int ,
                                       const char* value);

bool trainerCampUiRecentlyActive();

long trainerCampUiAgeMs();

void clearCampPriceWindow();

bool trainerCampSessionOpen();

void __fastcall midDragDropInterfDtorHooked(game::CMidDragDropInterf* thisptr, int );

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
