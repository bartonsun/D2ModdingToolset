#ifndef ITEMPOTIONHOOKS_H
#define ITEMPOTIONHOOKS_H

#include "itempotionboost.h"
#include "itempotionboostperm.h"
#include "itempotionboosttemp.h"
#include "itempotionheal.h"
#include "itempotionrevive.h"
#include "midgardid.h"
#include "idview.h"

namespace game {
struct GlobalData;
struct CDBTable;

} // namespace game

namespace hooks {

struct CItemPotionHealPatched : public game::CItemPotionHeal
{
    int itemCat;
    int hpPotion;
    game::CMidgardID modPotion;
};

struct CItemPotionBoostTempPatched : public game::CItemPotionBoostTemp
{
    int itemCat;
    int hpPotion;
    game::CMidgardID modPotion;
};

struct CItemPotionBoostPermPatched : public game::CItemPotionBoostPerm
{
    int itemCat;
    int hpPotion;
    game::CMidgardID modPotion;
};

struct CItemPotionRevivePatched : public game::CItemPotionRevive
{
    int itemCat;
    int hpPotion;
    game::CMidgardID modPotion;
};

game::CItemPotionHeal* __fastcall itemPotionHealCtorHooked(game::CItemPotionHeal* thisptr,
                                                           int /*%edx*/,
                                                           game::CDBTable* dbTable,
                                                           const game::GlobalData** globalData);

game::CItemPotionBoostTemp* __fastcall itemPotionBoostTempCtorHooked(
    game::CItemPotionBoostTemp* thisptr,
    int /*%edx*/,
    game::CDBTable* dbTable,
    const game::GlobalData** globalData);

game::CItemPotionBoostPerm* __fastcall itemPotionBoostPermCtorHooked(
    game::CItemPotionBoostPerm* thisptr,
    int /*%edx*/,
    game::CDBTable* dbTable,
    const game::GlobalData** globalData);

game::CItemPotionRevive* __fastcall itemPotionReviveCtorHooked(game::CItemPotionRevive* thisptr,
                                                               int /*%edx*/,
                                                               game::CDBTable* dbTable,
                                                               const game::GlobalData** globalData);

} // namespace hooks

#endif // ITEMPOTIONHOOKS_H
