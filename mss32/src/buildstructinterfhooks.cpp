#include "buildstructinterfhooks.h"
#include "borderedimg.h"
#include "buildstructinterf.h"
#include "button.h"
#include "dialoginterf.h"
#include "game.h"
#include "globaldata.h"
#include "interfaceutils.h"
#include "mempool.h"
#include "midgardid.h"
#include "mqrect.h"
#include "multilayerimg.h"
#include "originalfunctions.h"
#include "pictureinterf.h"
#include "textboxinterf.h"
#include "usracialsoldier.h"
#include "ussoldier.h"
#include "usunitimpl.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace hooks {
namespace {

struct UpgradePath
{
    game::CMidgardID unitId;
    std::string sourceName;
    std::string targetName;
    int level;
};

struct LayoutState
{
    game::CBuildStructInterf* owner{};
    game::CDialogInterf* dialog{};
    game::CMqRect faceArea{};
    game::CMqRect unitInfoArea{};
    bool active{};
};

LayoutState layoutState;

std::vector<UpgradePath> getUpgradePaths(const game::CMidgardID& buildingId)
{
    using namespace game;

    std::vector<UpgradePath> result;
    const auto& globalApi = GlobalDataApi::get();
    const auto& idApi = CMidgardIDApi::get();
    const auto& functions = gameFunctions();
    const auto globalDataPtr = globalApi.getGlobalData();
    if (!globalDataPtr) {
        return result;
    }
    const auto globalData = *globalDataPtr;

    if (!globalData || !globalData->units || !globalData->units->map) {
        return result;
    }

    const auto& units = globalData->units->map->data;
    for (auto it = units.bgn; it != units.end; ++it) {
        if (idApi.getType(&it->first) != IdType::UnitGlobal) {
            continue;
        }

        auto racialSoldier = functions.castUnitImplToRacialSoldier(it->second);
        if (!racialSoldier) {
            continue;
        }

        const auto upgradeBuildingId = racialSoldier->vftable->getUpgradeBuildingId(racialSoldier);
        if (!upgradeBuildingId || *upgradeBuildingId != buildingId) {
            continue;
        }

        auto target = functions.castUnitImplToSoldier(it->second);
        const auto previousId = racialSoldier->vftable->getPrevUnitImplId(racialSoldier);
        auto previousImpl = previousId ? static_cast<TUsUnitImpl*>(
                                             globalApi.findById(globalData->units, previousId))
                                       : nullptr;
        auto source = previousImpl ? functions.castUnitImplToSoldier(previousImpl) : nullptr;
        if (!source || !target) {
            continue;
        }

        const auto sourceName = source->vftable->getName(source);
        const auto targetName = target->vftable->getName(target);
        result.push_back(UpgradePath{it->first, sourceName ? sourceName : "",
                                     targetName ? targetName : "",
                                     target->vftable->getLevel(target)});
    }

    std::sort(result.begin(), result.end(),
              [](const UpgradePath& first, const UpgradePath& second) {
                  if (first.level != second.level) {
                      return first.level < second.level;
                  }
                  if (first.sourceName != second.sourceName) {
                      return first.sourceName < second.sourceName;
                  }
                  return first.unitId < second.unitId;
              });

    result.erase(std::unique(result.begin(), result.end(),
                             [](const UpgradePath& first, const UpgradePath& second) {
                                 return first.unitId == second.unitId;
                             }),
                 result.end());
    return result;
}

std::string toRoman(int value)
{
    static const std::pair<int, const char*> digits[] = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"}, {50, "L"},
        {40, "XL"},  {10, "X"},   {9, "IX"},  {5, "V"},    {4, "IV"},  {1, "I"},
    };

    std::string result;
    for (const auto& [number, text] : digits) {
        while (value >= number) {
            result += text;
            value -= number;
        }
    }
    return result;
}

std::string getPathsText(const std::vector<UpgradePath>& paths)
{
    std::string names;
    for (const auto& path : paths) {
        if (!names.empty()) {
            names += '\n';
        }

        names += path.sourceName;
        names += " -> ";
        names += path.targetName;
        if (path.level > 0) {
            names += " [";
            names += toRoman(path.level);
            names += ']';
        }
    }

    return names;
}

std::string getTargetNames(const std::vector<UpgradePath>& paths)
{
    std::string result;
    for (const auto& path : paths) {
        if (!result.empty()) {
            result += ", ";
        }
        result += path.targetName;
    }
    return result;
}

void restoreLayout(game::CBuildStructInterf* interf)
{
    if (!layoutState.active) {
        return;
    }

    if (layoutState.owner == interf && layoutState.dialog == interf->dialog) {
        const auto& dialogApi = game::CDialogInterfApi::get();
        auto face = dialogApi.findPicture(interf->dialog, "IMG_FACE");
        auto unitInfo = dialogApi.findTextBox(interf->dialog, "TXT_UNIT_INFO");
        if (face) {
            face->vftable->setArea(face, &layoutState.faceArea);
        }
        if (unitInfo) {
            unitInfo->vftable->setArea(unitInfo, &layoutState.unitInfoArea);
        }
    }

    layoutState.active = false;
}

game::CMultiLayerImg* createFacesImage(const std::vector<UpgradePath>& paths, int maxWidth)
{
    using namespace game;

    std::vector<std::pair<IMqImage2*, int>> faces;
    int faceWidth = 0;
    int totalWidth = 0;
    for (const auto& path : paths) {
        auto unitId = path.unitId;
        auto face = gameFunctions().createUnitFaceImage(&unitId, false);
        if (!face) {
            continue;
        }
        face->vftable->setLeftSide(face, false);

        auto framedFace = createBorderedImage(reinterpret_cast<IMqImage2*>(face),
                                              BorderType::UnitSmall);
        CMqPoint size{};
        framedFace->vftable->getSize(framedFace, &size);
        faceWidth = std::max(faceWidth, size.x);
        totalWidth += size.x;
        faces.emplace_back(framedFace, size.x);
    }

    if (faces.empty()) {
        return nullptr;
    }

    auto image = static_cast<CMultiLayerImg*>(Memory::get().allocate(sizeof(CMultiLayerImg)));
    const auto& multilayerApi = CMultiLayerImgApi::get();
    multilayerApi.constructor(image);

    constexpr int gap = 4;
    totalWidth += gap * (static_cast<int>(faces.size()) - 1);
    const bool overlap = totalWidth > maxWidth;
    const int step = overlap && faces.size() > 1
                         ? std::max(1,
                                    (maxWidth - faceWidth) / (static_cast<int>(faces.size()) - 1))
                         : 0;
    int offset = 0;
    for (const auto& [face, width] : faces) {
        multilayerApi.addImage(image, face, offset, 0);
        offset += overlap ? step : width + gap;
    }

    return image;
}

void showUpgradePaths(game::CBuildStructInterf* interf,
                      const std::vector<UpgradePath>& paths,
                      const std::vector<UpgradePath>& hiddenPaths)
{
    using namespace game;

    const auto& dialogApi = CDialogInterfApi::get();
    auto upgraded = dialogApi.findTextBox(interf->dialog, "TXT_UPGRADED");
    auto unitInfo = dialogApi.findTextBox(interf->dialog, "TXT_UNIT_INFO");
    auto info = dialogApi.findTextBox(interf->dialog, "TXT_INFO");
    auto status = dialogApi.findTextBox(interf->dialog, "TXT_STATUS");
    auto face = dialogApi.findPicture(interf->dialog, "IMG_FACE");
    auto buildButton = dialogApi.findButton(interf->dialog, "BTN_BUILD");
    if (!upgraded || !unitInfo || !info || !status || !face || !buildButton) {
        return;
    }

    layoutState.owner = interf;
    layoutState.dialog = interf->dialog;
    layoutState.faceArea = *face->vftable->getArea(face);
    layoutState.unitInfoArea = *unitInfo->vftable->getArea(unitInfo);
    layoutState.active = true;

    std::string infoText = info->data->text.string;
    if (!infoText.empty()) {
        infoText += '\n';
    }
    infoText += getPathsText(hiddenPaths);
    CTextBoxInterfApi::get().setString(info, infoText.c_str());

    std::string upgradedText = upgraded->data->text.string;
    if (!upgradedText.empty()) {
        upgradedText += ", ";
    }
    upgradedText += getTargetNames(hiddenPaths);
    CTextBoxInterfApi::get().setString(upgraded, upgradedText.c_str());

    const auto infoArea = *info->vftable->getArea(info);
    auto faceArea = layoutState.faceArea;
    const int maxWidth = infoArea.right - faceArea.left;
    auto facesImage = createFacesImage(paths, maxWidth);
    if (!facesImage) {
        return;
    }

    faceArea.right = infoArea.right;
    face->vftable->setArea(face, &faceArea);

    auto unitInfoArea = *status->vftable->getArea(status);
    unitInfoArea.top = unitInfoArea.bottom;
    unitInfoArea.bottom = buildButton->vftable->getArea(reinterpret_cast<CInterface*>(buildButton))
                              ->top;
    unitInfo->vftable->setArea(unitInfo, &unitInfoArea);
    setCenteredImage(face, facesImage);
}

} // namespace

void __fastcall buildStructInterfUpdateBuildingInfoHooked(game::CBuildStructInterf* thisptr,
                                                          int /*%edx*/)
{
    restoreLayout(thisptr);
    getOriginalFunctions().buildStructInterfUpdateBuildingInfo(thisptr);

    if (!thisptr || !thisptr->dialog || !thisptr->data) {
        return;
    }

    const auto& buildingId = thisptr->data->buildingId;
    if (game::CMidgardIDApi::get().getType(&buildingId) != game::IdType::Building) {
        return;
    }

    if (!thisptr->data->displayedUnit) {
        return;
    }

    auto paths = getUpgradePaths(buildingId);
    auto hiddenPaths = paths;
    const auto& displayedUnitId = thisptr->data->displayedUnit->id;
    hiddenPaths.erase(std::remove_if(hiddenPaths.begin(), hiddenPaths.end(),
                                     [&displayedUnitId](const UpgradePath& path) {
                                         return path.unitId == displayedUnitId;
                                     }),
                      hiddenPaths.end());

    if (!hiddenPaths.empty() && hiddenPaths.size() < paths.size()) {
        showUpgradePaths(thisptr, paths, hiddenPaths);
    }
}

} // namespace hooks
