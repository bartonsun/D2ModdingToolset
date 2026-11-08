#include "upgradeportraitlayout.h"
#include <cassert>
#include <iostream>

using game::CMqPoint;
using game::CMqRect;
using hooks::upgradePortraitLayout::makeRow;
using hooks::upgradePortraitLayout::placeRow;
using hooks::upgradePortraitLayout::makeFittedRow;
using hooks::upgradePortraitLayout::parchmentArea;

void checkParchmentFit()
{
    const CMqRect original{476, 250, 560, 349};
    // Regression: the dialog extends to 800 but the paper ends at TXT_INFO.right.
    const auto paper = parchmentArea(original, {477, 50, 767, 230}, {0, 0, 800, 600});
    assert(paper.left == 481 && paper.right == 763);
    const auto pair = makeFittedRow({{164, 99}, {164, 99}}, 282, 99, 8);
    assert(pair.fits && pair.portraits.size() == 2);
    assert(pair.size.x == 282 && pair.size.y == 83);
    for (const auto& portrait : pair.portraits) {
        assert(portrait.right - portrait.left == 137);
        assert(portrait.bottom - portrait.top == 83);
    }
    assert(pair.portraits[1].left - pair.portraits[0].right == 8);
    CMqRect area{};
    assert(placeRow(pair, original, paper, area));
    assert(area.left >= 481 && area.right <= 763);
    assert(area.top >= original.top && area.bottom <= original.bottom);
    CMqRect largeArea{};
    assert(placeRow(pair, {476, 250, 640, 349}, paper, largeArea));
    assert(largeArea.left == 481 && largeArea.right == 763);
    assert(largeArea.top == 258 && largeArea.bottom == 341);

    const auto mixed = makeFittedRow({{164, 99}, {84, 99}}, 282, 99, 8);
    assert(mixed.fits && mixed.size.x == 256 && mixed.size.y == 99);
    assert(mixed.portraits[0].right == 164); // No unnecessary downscale or crop.
    const auto shortArea = makeFittedRow({{164, 99}, {164, 99}}, 282, 60, 8);
    assert(shortArea.fits && shortArea.size.y <= 60 && shortArea.size.x <= 282);
    assert(!makeFittedRow({{164, 99}, {164, 99}}, 8, 99, 8).fits);
    assert(!makeFittedRow({{164, 99}, {0, 99}}, 282, 99, 8).fits);
    assert(!makeFittedRow({}, 282, 99, 8).fits);

    // Translation does not enlarge the available paper or change the scale.
    const auto moved = parchmentArea({588, 287, 672, 386},
                                     {589, 87, 879, 267}, {112, 37, 912, 637});
    CMqRect movedArea{};
    assert(placeRow(pair, {588, 287, 672, 386}, moved, movedArea));
    assert(movedArea.left == area.left + 112 && movedArea.top == area.top + 37);
}

void checkRow(const std::vector<CMqPoint>& sizes)
{
    const CMqRect original{476, 250, 560, 349};
    const CMqRect available{441, 250, 796, 349};
    const auto row = makeRow(sizes, available.right - available.left, 4);
    CMqRect area{};
    assert(placeRow(row, original, available, area));
    assert(area.left >= available.left && area.right <= available.right);
    assert(area.top >= available.top && area.bottom <= available.bottom);
    assert(area.right - area.left == row.size.x);
    assert(area.bottom - area.top == row.size.y);
    for (std::size_t i = 0; i < row.portraits.size(); ++i) {
        const auto& portrait = row.portraits[i];
        assert(portrait.left >= 0 && portrait.right <= row.size.x);
        assert(portrait.top >= 0 && portrait.bottom <= row.size.y);
        assert(portrait.right - portrait.left == sizes[i].x);
        assert(portrait.bottom - portrait.top == sizes[i].y);
        if (i) {
            assert(row.portraits[i - 1].right <= portrait.left);
        }
    }

    auto translatedOriginal = original;
    auto translatedAvailable = available;
    translatedOriginal.left += 112;
    translatedOriginal.right += 112;
    translatedOriginal.top += 37;
    translatedOriginal.bottom += 37;
    translatedAvailable.left += 112;
    translatedAvailable.right += 112;
    translatedAvailable.top += 37;
    translatedAvailable.bottom += 37;
    CMqRect translatedArea{};
    assert(placeRow(row, translatedOriginal, translatedAvailable, translatedArea));
    assert(translatedArea.left == area.left + 112);
    assert(translatedArea.right == area.right + 112);
    assert(translatedArea.top == area.top + 37);
    assert(translatedArea.bottom == area.bottom + 37);
}

int main()
{
    checkParchmentFit();
    checkRow({{84, 99}, {84, 99}});
    checkRow({{164, 99}, {84, 99}});
    checkRow({{84, 99}, {164, 99}});
    checkRow({{164, 99}, {164, 99}});
    checkRow({{160, 99}, {160, 99}});
    checkRow({{84, 99}, {84, 99}, {84, 99}});

    const auto nativeLargePair = makeRow({{164, 99}, {164, 99}}, 355, 4);
    CMqRect nativeLargeArea{};
    assert(placeRow(nativeLargePair, {476, 250, 560, 349}, {441, 250, 796, 349},
                     nativeLargeArea));
    assert(nativeLargeArea.left == 441 && nativeLargeArea.right == 773);
    const auto compressed = makeRow({{164, 99}, {164, 99}}, 329, 4);
    assert(compressed.fits && compressed.size.x == 329);
    assert(compressed.portraits[1].left == 165);
    const auto tooWide = makeRow({{170, 99}, {170, 99}}, 326, 4);
    assert(!tooWide.fits);
    CMqRect area{};
    assert(!placeRow(tooWide, {476, 250, 560, 349}, {441, 250, 767, 349}, area));
    assert(!placeRow(makeRow({{84, 100}}, 326, 4), {476, 250, 560, 349},
                     {441, 250, 767, 349}, area));
    assert(!makeRow({}, 326, 4).fits);
    assert(!makeRow({{0, 99}}, 326, 4).fits);

    const auto popup = makeRow({{164, 99}, {65, 34}, {164, 99}}, 447, 6);
    assert(popup.fits && popup.size.x == 405 && popup.size.y == 99);
    assert(popup.portraits[0].right < popup.portraits[1].left);
    assert(popup.portraits[1].right < popup.portraits[2].left);
    const int rowsHeight = popup.size.y * 2 + 6;
    assert(71 + 4 + 28 + 4 + 80 + 4 + rowsHeight <= 410 - 4);
    std::cout << "Upgrade portrait layout regression tests passed\n";
}
