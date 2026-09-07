#include "trainingcostapi.h"
#include "version.h"
#include <array>
#include <iterator>

namespace game::TrainingCostApi {

static std::array<Api, 4> functions = {{
    // Akella: call sites measured in Discipl2.exe (4187648, slasher_mns_2_4).
    // trainUnitAtTrainer 0x5d8f5d builds the cost copy at 0x5d9004 (returns to
    // 0x5d9009); canAffordTrainCheck 0x46d4a4 builds it at 0x46d54f (returns
    // to 0x46d554); the price multiply sits at 0x5d901f (returns to 0x5d9024).
    Api{
        (Api::TrainUnitAtTrainer)0x5d8f5d,
        (Api::TrainUiAction)0x5009c8,
        (Api::CanAffordTrainCheck)0x46d4a4,
        (Api::ApplyTrainAction)0x46d999,
        (const void*)0x5d9009,
        (const void*)0x46d554,
        (const void*)0x5d9024,
    },
    Api{
        (Api::TrainUnitAtTrainer)0x5d8f5d,
        (Api::TrainUiAction)0x5009c8,
        (Api::CanAffordTrainCheck)0x46d4a4,
        (Api::ApplyTrainAction)0x46d999,
        (const void*)0x5d9009,
        (const void*)0x46d554,
        (const void*)0x5d9024,
    },
    // Gog: cost copy sites are the Akella addresses minus the same function
    // pair deltas the four hooks use (train 0x12f2, afford 0x700). GoG exe is
    // not on hand -- derived, same caveat as the hook rows above it.
    Api{
        (Api::TrainUnitAtTrainer)0x5d7c6b,
        (Api::TrainUiAction)0x4ffcb8,
        (Api::CanAffordTrainCheck)0x46cda4,
        (Api::ApplyTrainAction)0x46d299,
        (const void*)0x5d7d17,
        (const void*)0x46ce54,
        (const void*)0x5d7d32,
    },
    Api{
        (Api::TrainUnitAtTrainer)0,
        (Api::TrainUiAction)0,
        (Api::CanAffordTrainCheck)0,
        (Api::ApplyTrainAction)0,
        (const void*)0,
        (const void*)0,
        (const void*)0,
    },
}};

Api& get()
{
    return functions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::TrainingCostApi

namespace game::TrainCampTextApi {

static std::array<Api, 4> textFunctions = {{
    Api{reinterpret_cast<Api::SetPartyTrainingText>(0x4a6e68)},
    Api{reinterpret_cast<Api::SetPartyTrainingText>(0x4a6e68)},
    Api{reinterpret_cast<Api::SetPartyTrainingText>(0x4a67d5)},
    Api{reinterpret_cast<Api::SetPartyTrainingText>(0)},
}};

Api& get()
{
    return textFunctions[static_cast<int>(hooks::gameVersion())];
}

} // namespace game::TrainCampTextApi
