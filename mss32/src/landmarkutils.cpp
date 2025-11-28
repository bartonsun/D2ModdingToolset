#include "landmarkutils.h"
#include "globaldata.h"
#include "landmark.h"
#include "utils.h"
#include <spdlog/spdlog.h>

namespace hooks {

bool isLandmarkMountain(const game::CMidgardID* landmarkTypeId)
{
    using namespace game;

    if (!landmarkTypeId) {
        return false;
    }

    const GlobalData* global = *GlobalDataApi::get().getGlobalData();
    if (!global) {
        spdlog::error("Global data is not available");
        return false;
    }

    if (!global->landmarks || !*global->landmarks) {
        spdlog::error("Landmarks global data is not loaded");
        return false;
    }

    // Получаем доступ к данным landmarks аналогично spells
    const auto& landmarks = (*global->landmarks)->data;

    // Перебираем все landmarks в поисках нужного ID
    for (const auto* i = landmarks.bgn; i != landmarks.end; ++i) {
        const TLandmark* landmark = i->second;

        if (landmark->id == *landmarkTypeId) {
            if (landmark->data) {
                return landmark->data->mountain;
            } else {
                spdlog::error("Global landmark {:s} has null data", idToString(landmarkTypeId));
                return false;
            }
        }
    }

    spdlog::error("Could not find global landmark {:s} in landmarks collection",
                  idToString(landmarkTypeId));
    return false;
}

} // namespace hooks