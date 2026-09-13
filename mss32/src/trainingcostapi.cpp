#include "trainingcostapi.h"
#include "version.h"
#include <array>
#include <iterator>

namespace game::TrainingCostApi {

static std::array<Api, 4> functions = {{
    Api{
        (Api::TrainUnitAtTrainer)0x5d8f5d,
        (Api::TrainUiAction)0x5009c8,
        (Api::CanAffordTrainCheck)0x46d4a4,
        (Api::ApplyTrainAction)0x46d999,
        (Api::AddExperience)0x5e8942,
        (const void*)0x5d9009,
        (const void*)0x46d554,
        (const void*)0x5d9084,
    },
    Api{
        (Api::TrainUnitAtTrainer)0x5d8f5d,
        (Api::TrainUiAction)0x5009c8,
        (Api::CanAffordTrainCheck)0x46d4a4,
        (Api::ApplyTrainAction)0x46d999,
        (Api::AddExperience)0x5e8942,
        (const void*)0x5d9009,
        (const void*)0x46d554,
        (const void*)0x5d9084,
    },
    Api{
        (Api::TrainUnitAtTrainer)0x5d7c6b,
        (Api::TrainUiAction)0x4ffcb8,
        (Api::CanAffordTrainCheck)0x46cda4,
        (Api::ApplyTrainAction)0x46d299,
        (Api::AddExperience)0x5e7650,
        (const void*)0x5d7d17,
        (const void*)0x46ce54,
        (const void*)0x5d7d92,
    },
    Api{
        (Api::TrainUnitAtTrainer)0,
        (Api::TrainUiAction)0,
        (Api::CanAffordTrainCheck)0,
        (Api::ApplyTrainAction)0,
        (Api::AddExperience)0,
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
