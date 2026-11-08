#ifndef UPGRADEPORTRAITLAYOUT_H
#define UPGRADEPORTRAITLAYOUT_H

#include "mqpoint.h"
#include "mqrect.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace hooks::upgradePortraitLayout {

struct Row
{
    game::CMqPoint size{};
    std::vector<game::CMqRect> portraits;
    bool fits{};
};

inline Row makeRow(const std::vector<game::CMqPoint>& sizes, int width, int preferredGap)
{
    Row result;
    if (sizes.empty() || width <= 0) {
        return result;
    }

    for (const auto& size : sizes) {
        if (size.x <= 0 || size.y <= 0) {
            return {};
        }
        result.size.x += size.x;
        result.size.y = std::max(result.size.y, size.y);
    }

    const int gaps = static_cast<int>(sizes.size()) - 1;
    const int gap = gaps ? std::max(0, std::min(preferredGap, (width - result.size.x) / gaps))
                         : 0;
    result.size.x += gap * gaps;
    result.fits = result.size.x <= width;
    int left = 0;
    for (const auto& size : sizes) {
        const int top = (result.size.y - size.y) / 2;
        result.portraits.push_back({left, top, left + size.x, top + size.y});
        left += size.x + gap;
    }
    return result;
}

// Fit the entire row to the parchment, not to the outer dialog ornament.
// All portraits share one scale factor; equal-size sources stay equal-size.
inline Row makeFittedRow(const std::vector<game::CMqPoint>& sizes,
                         int width, int height, int gap)
{
    if (sizes.empty() || width <= 0 || height <= 0 || gap < 0) {
        return {};
    }
    std::int64_t totalWidth = 0;
    int maxHeight = 0;
    for (const auto& size : sizes) {
        if (size.x <= 0 || size.y <= 0) {
            return {};
        }
        totalWidth += size.x;
        maxHeight = std::max(maxHeight, size.y);
    }
    const auto gapsWidth = static_cast<std::int64_t>(sizes.size() - 1) * gap;
    if (gapsWidth >= width) {
        return {};
    }
    const double scale = std::min({1.0, static_cast<double>(width - gapsWidth) / totalWidth,
                                   static_cast<double>(height) / maxHeight});
    std::vector<game::CMqPoint> fitted;
    fitted.reserve(sizes.size());
    for (const auto& size : sizes) {
        const int x = static_cast<int>(size.x * scale);
        const int y = std::min(height, static_cast<int>(size.y * scale + 0.5));
        if (x < 1 || y < 1) {
            return {};
        }
        fitted.push_back({x, y});
    }
    return makeRow(fitted, width, gap);
}

inline game::CMqRect parchmentArea(const game::CMqRect& original,
                                   const game::CMqRect& info,
                                   const game::CMqRect& dialog)
{
    constexpr int inset = 4;
    return {std::max(info.left, dialog.left) + inset, original.top,
            std::min(info.right, dialog.right) - inset, original.bottom};
}

inline bool placeRow(const Row& row,
                     const game::CMqRect& original,
                     const game::CMqRect& available,
                     game::CMqRect& area)
{
    if (!row.fits || row.portraits.empty() || row.size.x > available.right - available.left
        || row.size.y > available.bottom - available.top) {
        return false;
    }

    const auto& first = row.portraits.front();
    const int preferredLeft = original.left
                              + (original.right - original.left - first.right + first.left) / 2;
    const int preferredTop = original.top + (original.bottom - original.top - row.size.y) / 2;
    const int left = std::clamp(preferredLeft, available.left, available.right - row.size.x);
    const int top = std::clamp(preferredTop, available.top, available.bottom - row.size.y);
    area = {left, top, left + row.size.x, top + row.size.y};
    return true;
}

}

#endif
