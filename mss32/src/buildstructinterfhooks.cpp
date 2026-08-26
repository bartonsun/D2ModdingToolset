#include "buildstructinterfhooks.h"
#include "borderedimg.h"
#include "buildstructinterf.h"
#include "dialoginterf.h"
#include "formattedtext.h"
#include "game.h"
#include "globaldata.h"
#include "interfaceutils.h"
#include "mempool.h"
#include "midgardid.h"
#include "mqrect.h"
#include "multilayerimg.h"
#include "originalfunctions.h"
#include "pictureinterf.h"
#include "smartptr.h"
#include "textboxinterf.h"
#include "usracialsoldier.h"
#include "ussoldier.h"
#include "usunitimpl.h"
#include "utils.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace hooks {
namespace {

struct UpgradePath
{
    game::CMidgardID sourceId;
    game::CMidgardID sourceCanonicalId;
    game::CMidgardID unitId;
    game::CMidgardID unitCanonicalId;
    std::string sourceName;
    std::string targetName;
    int level;
};

struct LayoutState
{
    struct Portrait
    {
        game::CMqRect area{};
        const game::TUsUnitImpl* unit{};
    };

    game::CBuildStructInterf* owner{};
    game::CDialogInterf* dialog{};
    game::CMqRect faceArea{};
    game::CMqRect upgradedArea{};
    std::vector<Portrait> portraits;
    bool active{};
};

LayoutState layoutState;

game::RttiInfo<game::CInterfaceVftable> buildStructRttiInfo{};
game::CInterfaceVftable::HandleMouse buildStructHandleMouse{};

int callBuildStructHandleMouse(game::CBuildStructInterf* interf,
                               const game::TUsUnitImpl* unit,
                               std::uint32_t mouseButton,
                               const game::CMqPoint* mousePosition)
{
    const auto previousUnit = interf->data->displayedUnit;
    interf->data->displayedUnit = unit;
    game::CBuildStructInterfApi::get().unitFaceMouseButtonCallback(interf, mouseButton,
                                                                   mousePosition);
    interf->data->displayedUnit = previousUnit;
    return 1;
}

int __fastcall buildStructHandleMouseHooked(game::CInterface* thisptr,
                                            int /*%edx*/,
                                            std::uint32_t mouseButton,
                                            const game::CMqPoint* mousePosition)
{
    if (mouseButton != WM_RBUTTONDOWN) {
        return buildStructHandleMouse(thisptr, mouseButton, mousePosition);
    }

    auto interf = reinterpret_cast<game::CBuildStructInterf*>(thisptr);
    if (!interf->dialog || !interf->data || !mousePosition) {
        return 1;
    }

    if (layoutState.active && layoutState.owner == interf
        && layoutState.dialog == interf->dialog) {
        for (const auto& portrait : layoutState.portraits) {
            if (portrait.unit
                && game::MqRectApi::get().ptInRect(&portrait.area, mousePosition)) {
                return callBuildStructHandleMouse(interf, portrait.unit, mouseButton,
                                                  mousePosition);
            }
        }
        return 1;
    }

    const auto face = game::CDialogInterfApi::get().findPicture(interf->dialog, "IMG_FACE");
    if (face && interf->data->displayedUnit
        && game::MqRectApi::get().ptInRect(face->vftable->getArea(face), mousePosition)) {
        return callBuildStructHandleMouse(interf, interf->data->displayedUnit, mouseButton,
                                          mousePosition);
    }

    return 1;
}

void installBuildStructMouseHandler(game::CBuildStructInterf* interf)
{
    auto base = reinterpret_cast<game::CInterface*>(interf);
    if (base->vftable == &buildStructRttiInfo.vftable) {
        return;
    }

    if (!buildStructHandleMouse) {
        buildStructHandleMouse = base->vftable->handleMouse;
        replaceRttiInfo(buildStructRttiInfo, base->vftable, true);
        buildStructRttiInfo.vftable.handleMouse =
            reinterpret_cast<game::CInterfaceVftable::HandleMouse>(
                buildStructHandleMouseHooked);
    }

    base->vftable = &buildStructRttiInfo.vftable;
}

game::CMidgardID getCanonicalUnitId(const game::TUsUnitImpl* unit)
{
    using namespace game;

    if (!unit) {
        return emptyId;
    }

    auto soldier = gameFunctions().castUnitImplToSoldier(unit);
    if (soldier) {
        const auto baseId = soldier->vftable->getBaseUnitImplId(soldier);
        if (baseId && CMidgardIDApi::get().getType(baseId) == IdType::UnitGlobal) {
            return *baseId;
        }
    }

    return unit->id;
}

std::vector<UpgradePath> getUpgradePaths(const game::CMidgardID& buildingId,
                                         const game::CMidgardID* preferredUnitId = nullptr)
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
        result.push_back(UpgradePath{*previousId, getCanonicalUnitId(previousImpl), it->second->id,
                                     getCanonicalUnitId(it->second), sourceName ? sourceName : "",
                                     targetName ? targetName : "",
                                     target->vftable->getLevel(target)});
    }

    std::vector<UpgradePath> uniquePaths;
    for (auto& path : result) {
        const auto samePath = [&path](const UpgradePath& other) {
            return path.sourceCanonicalId == other.sourceCanonicalId
                   && path.unitCanonicalId == other.unitCanonicalId
                   && path.sourceName == other.sourceName && path.targetName == other.targetName
                   && path.level == other.level;
        };
        const auto duplicate = std::find_if(uniquePaths.begin(), uniquePaths.end(), samePath);
        if (duplicate == uniquePaths.end()) {
            uniquePaths.push_back(std::move(path));
            continue;
        }

        const auto representativeRank = [preferredUnitId](const UpgradePath& candidate) {
            int rank = 0;
            if (preferredUnitId && candidate.unitId == *preferredUnitId) {
                rank += 4;
            }
            if (candidate.unitId == candidate.unitCanonicalId) {
                rank += 2;
            }
            if (candidate.sourceId == candidate.sourceCanonicalId) {
                rank += 1;
            }
            return rank;
        };
        if (representativeRank(path) > representativeRank(*duplicate)) {
            *duplicate = std::move(path);
        }
    }
    result = std::move(uniquePaths);

    std::sort(result.begin(), result.end(),
              [](const UpgradePath& first, const UpgradePath& second) {
                  if (first.level != second.level) {
                      return first.level < second.level;
                  }
                  if (first.sourceName != second.sourceName) {
                      return first.sourceName < second.sourceName;
                  }
                  if (first.sourceCanonicalId != second.sourceCanonicalId) {
                      return first.sourceCanonicalId < second.sourceCanonicalId;
                  }
                  return first.unitCanonicalId < second.unitCanonicalId;
              });
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

int getTextWidth(game::CTextBoxInterf* textBox, const std::string& text)
{
    using namespace game;

    FormattedTextPtr formattedText;
    IFormattedTextApi::get().getFormattedText(&formattedText);
    if (!formattedText.data) {
        return 0;
    }

    std::string formatted = textBox->data->format.string;
    formatted += text;
    const int width = formattedText.data->vftable->getTextWidth(formattedText.data,
                                                                formatted.c_str());
    SmartPointerApi::get().createOrFree(reinterpret_cast<SmartPointer*>(&formattedText), nullptr);
    return width;
}

std::string fitSingleLineText(game::CTextBoxInterf* textBox,
                              const std::string& text,
                              int width)
{
    if (getTextWidth(textBox, text) <= width) {
        return text;
    }

    const std::string smallFont = "\\fSmall;";
    if (getTextWidth(textBox, smallFont + text) <= width) {
        return smallFont + text;
    }

    std::string shortened = text;
    while (!shortened.empty()) {
        shortened.pop_back();
        const auto candidate = smallFont + shortened + "...";
        if (getTextWidth(textBox, candidate) <= width) {
            return candidate;
        }
    }

    return smallFont + "...";
}

void restoreLayout(game::CBuildStructInterf* interf)
{
    if (!layoutState.active) {
        layoutState.portraits.clear();
        return;
    }

    if (layoutState.owner == interf && layoutState.dialog == interf->dialog) {
        const auto& dialogApi = game::CDialogInterfApi::get();
        auto face = dialogApi.findPicture(interf->dialog, "IMG_FACE");
        auto upgraded = dialogApi.findTextBox(interf->dialog, "TXT_UPGRADED");
        if (face) {
            face->vftable->setArea(face, &layoutState.faceArea);
        }
        if (upgraded) {
            upgraded->vftable->setArea(upgraded, &layoutState.upgradedArea);
        }
    }

    layoutState.portraits.clear();
    layoutState.active = false;
}

struct FramedFace
{
    game::IMqImage2* image{};
    game::CMqPoint size{};
    const game::TUsUnitImpl* unit{};
};

FramedFace createFramedFace(const game::CMidgardID& unitId, bool leftSide)
{
    using namespace game;

    const auto globalDataPtr = GlobalDataApi::get().getGlobalData();
    const auto globalData = globalDataPtr ? *globalDataPtr : nullptr;
    auto unit = globalData && globalData->units
                    ? static_cast<TUsUnitImpl*>(
                          GlobalDataApi::get().findById(globalData->units, &unitId))
                    : nullptr;
    auto soldier = unit ? gameFunctions().castUnitImplToSoldier(unit) : nullptr;
    const bool smallUnit = !soldier || soldier->vftable->getSizeSmall(soldier);

    auto face = gameFunctions().createUnitFaceImage(const_cast<CMidgardID*>(&unitId), false);
    if (!face) {
        return {};
    }
    face->vftable->setLeftSide(face, leftSide);

    const auto border = smallUnit ? BorderType::UnitSmall : BorderType::UnitLarge;
    auto framedFace = createBorderedImage(reinterpret_cast<IMqImage2*>(face), border);
    CMqPoint size{};
    framedFace->vftable->getSize(framedFace, &size);
    return FramedFace{framedFace, size, unit};
}

FramedFace createFramedFace(const game::CMidgardID& unitId,
                            const game::CMidgardID& canonicalId,
                            bool leftSide)
{
    auto face = createFramedFace(unitId, leftSide);
    if (!face.image && canonicalId != unitId) {
        face = createFramedFace(canonicalId, leftSide);
    }
    return face;
}

struct FacesImage
{
    game::CMultiLayerImg* image{};
    game::CMqPoint size{};
    std::vector<LayoutState::Portrait> portraits;
};

FacesImage createFacesImage(const std::vector<UpgradePath>& paths)
{
    using namespace game;

    struct Face
    {
        IMqImage2* image;
        CMqPoint size;
        const TUsUnitImpl* unit;
    };

    std::vector<Face> faces;
    int totalWidth = 0;
    int maxHeight = 0;
    for (const auto& path : paths) {
        const auto framedFace = createFramedFace(path.unitId, path.unitCanonicalId, false);
        if (!framedFace.image) {
            continue;
        }
        totalWidth += framedFace.size.x;
        maxHeight = std::max(maxHeight, framedFace.size.y);
        faces.push_back(Face{framedFace.image, framedFace.size, framedFace.unit});
    }

    if (faces.empty()) {
        return {};
    }

    auto image = static_cast<CMultiLayerImg*>(Memory::get().allocate(sizeof(CMultiLayerImg)));
    const auto& multilayerApi = CMultiLayerImgApi::get();
    multilayerApi.constructor(image);

    constexpr int gap = 4;
    totalWidth += gap * (static_cast<int>(faces.size()) - 1);
    int offset = 0;
    std::vector<LayoutState::Portrait> portraits;
    portraits.reserve(faces.size());
    for (std::size_t i = 0; i < faces.size(); ++i) {
        const auto& face = faces[i];
        const int top = (maxHeight - face.size.y) / 2;
        multilayerApi.addImage(image, face.image, offset, top);
        portraits.push_back(LayoutState::Portrait{
            CMqRect{offset, top, offset + face.size.x, top + face.size.y}, face.unit});
        offset += face.size.x + gap;
    }
    image->data->size = CMqPoint{totalWidth, maxHeight};

    return FacesImage{image, image->data->size, std::move(portraits)};
}

void showUpgradePaths(game::CBuildStructInterf* interf,
                      const std::vector<UpgradePath>& paths,
                      const std::vector<UpgradePath>& hiddenPaths)
{
    using namespace game;

    const auto& dialogApi = CDialogInterfApi::get();
    auto upgraded = dialogApi.findTextBox(interf->dialog, "TXT_UPGRADED");
    auto info = dialogApi.findTextBox(interf->dialog, "TXT_INFO");
    auto face = dialogApi.findPicture(interf->dialog, "IMG_FACE");
    if (!upgraded || !info || !face) {
        return;
    }

    auto facesImage = createFacesImage(paths);
    if (!facesImage.image) {
        return;
    }

    layoutState.owner = interf;
    layoutState.dialog = interf->dialog;
    layoutState.faceArea = *face->vftable->getArea(face);
    layoutState.upgradedArea = *upgraded->vftable->getArea(upgraded);
    layoutState.portraits.clear();
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
    const auto infoArea = *info->vftable->getArea(info);
    auto upgradedArea = layoutState.upgradedArea;
    upgradedArea.right = infoArea.right;
    upgraded->vftable->setArea(upgraded, &upgradedArea);
    const auto fittedUpgradedText = fitSingleLineText(upgraded, upgradedText,
                                                      upgradedArea.right - upgradedArea.left);
    CTextBoxInterfApi::get().setString(upgraded, fittedUpgradedText.c_str());

    auto faceArea = layoutState.faceArea;
    faceArea.right = infoArea.right;
    face->vftable->setArea(face, &faceArea);

    const auto& firstPortrait = facesImage.portraits.front().area;
    const int firstPortraitWidth = firstPortrait.right - firstPortrait.left;
    const game::CMqPoint imageOffset{
        (layoutState.faceArea.right - layoutState.faceArea.left - firstPortraitWidth) / 2,
        (layoutState.faceArea.bottom - layoutState.faceArea.top - facesImage.size.y) / 2,
    };
    game::CPictureInterfApi::get().setImage(face, facesImage.image, &imageOffset);

    const int imageLeft = faceArea.left + imageOffset.x;
    const int imageTop = faceArea.top + imageOffset.y;
    layoutState.portraits = std::move(facesImage.portraits);
    for (auto& portrait : layoutState.portraits) {
        portrait.area.left += imageLeft;
        portrait.area.right += imageLeft;
        portrait.area.top += imageTop;
        portrait.area.bottom += imageTop;
    }
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

    installBuildStructMouseHandler(thisptr);

    auto unitInfo = game::CDialogInterfApi::get().findTextBox(thisptr->dialog, "TXT_UNIT_INFO");
    if (unitInfo) {
        game::CTextBoxInterfApi::get().setString(unitInfo, "");
    }

    const auto& buildingId = thisptr->data->buildingId;
    if (game::CMidgardIDApi::get().getType(&buildingId) != game::IdType::Building) {
        return;
    }

    if (!thisptr->data->displayedUnit) {
        return;
    }

    const auto displayedActualUnitId = thisptr->data->displayedUnit->id;
    const auto displayedUnitId = getCanonicalUnitId(thisptr->data->displayedUnit);
    auto paths = getUpgradePaths(buildingId, &displayedActualUnitId);
    const auto primary = std::find_if(paths.begin(), paths.end(),
                                      [&displayedUnitId](const UpgradePath& path) {
                                          return path.unitCanonicalId == displayedUnitId;
                                      });
    if (primary != paths.end() && primary != paths.begin()) {
        std::rotate(paths.begin(), primary, std::next(primary));
    }

    auto hiddenPaths = paths;
    hiddenPaths.erase(std::remove_if(hiddenPaths.begin(), hiddenPaths.end(),
                                     [&displayedUnitId](const UpgradePath& path) {
                                         return path.unitCanonicalId == displayedUnitId;
                                     }),
                      hiddenPaths.end());

    if (!hiddenPaths.empty() && hiddenPaths.size() < paths.size()) {
        showUpgradePaths(thisptr, paths, hiddenPaths);
    }
}

} // namespace hooks
