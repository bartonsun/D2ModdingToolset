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
    },
    Api{
        (Api::TrainUnitAtTrainer)0x5d8f5d,
        (Api::TrainUiAction)0x5009c8,
        (Api::CanAffordTrainCheck)0x46d4a4,
        (Api::ApplyTrainAction)0x46d999,
    },
    Api{
        (Api::TrainUnitAtTrainer)0x5d7c6b,
        (Api::TrainUiAction)0x4ffcb8,
        (Api::CanAffordTrainCheck)0x46cda4,
        (Api::ApplyTrainAction)0x46d299,
    },
    Api{
        (Api::TrainUnitAtTrainer)0,
        (Api::TrainUiAction)0,
        (Api::CanAffordTrainCheck)0,
        (Api::ApplyTrainAction)0,
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

namespace game::TrainingDiscountData {

static const Entry modifierTable[] = {
    {"g000um7548", 25},
    {"g100um7548", 25},
    {"g070um0097", 8},
    {"g006um0068", 15},
    {"g070um0281", 15},
    {"g070um0282", 20},
};

static const Entry itemTable[] = {
    {"g000ig3022", 25},
};

const Entry* modifiers(int& count)
{
    count = static_cast<int>(std::size(modifierTable));
    return modifierTable;
}

const Entry* items(int& count)
{
    count = static_cast<int>(std::size(itemTable));
    return itemTable;
}

const char* skaldDiscountModifier()
{
    return "g070um0097";
}

int skaldDiscountPerLevel()
{
    return 3;
}

} // namespace game::TrainingDiscountData
