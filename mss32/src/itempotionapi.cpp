#include "itembase.h"
#include "itempotionboost.h"
#include "itempotionboostperm.h"
#include "itempotionboosttemp.h"
#include "itempotionheal.h"
#include "itempotionrevive.h"
#include "version.h"
#include <array>

namespace game::CItemBaseDataApi {

static Api functions[4] = {
    {(Api::Constructor)0x5A7A6F}, // Akella
    {(Api::Constructor)0x5A7A6F}, // Russobit
    {(Api::Constructor)0x5A6CA7}, // Gog
    {(Api::Constructor)0},        // Scenario Editor
};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CItemBaseDataApi

namespace game::CItemBaseApi {

static Api functions[4] = {
    {(Api::Constructor)0x5A79A4}, // Akella
    {(Api::Constructor)0x5A79A4}, // Russobit
    {(Api::Constructor)0x5A6BDC}, // Gog
    {(Api::Constructor)0},        // Scenario Editor
};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CItemBaseApi

namespace game::CItemPotionHealApi {

static Api functions[4] = {
    {(Api::Constructor)0x59E3CE}, // Akella
    {(Api::Constructor)0x59E3CE}, // Russobit
    {(Api::Constructor)0x59D665}, // Gog
    {(Api::Constructor)0},        // Scenario Editor
};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CItemPotionHealApi

// --------------------------------------------------

namespace game::CItemPotionBoostTempApi {

static Api functions[4] = {
    {(Api::Constructor)0x59E377},
    {(Api::Constructor)0x59E377},
    {(Api::Constructor)0x59D60E},
    {(Api::Constructor)0},
};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CItemPotionBoostTempApi

// --------------------------------------------------

namespace game::CItemPotionBoostPermApi {

static Api functions[4] = {
    {(Api::Constructor)0x59E320},
    {(Api::Constructor)0x59E320},
    {(Api::Constructor)0x59D5B7},
    {(Api::Constructor)0},
};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CItemPotionBoostPermApi

// --------------------------------------------------

namespace game::CItemPotionReviveApi {

static Api functions[4] = {
    {(Api::Constructor)0x59E507},
    {(Api::Constructor)0x59E507},
    {(Api::Constructor)0x59D7AF},
    {(Api::Constructor)0},
};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::CItemPotionReviveApi
