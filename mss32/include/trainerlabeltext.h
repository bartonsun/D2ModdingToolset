/*
 * This file is part of the modding toolset for Disciples 2.
 *
 * Finding the gold number the camp is about to draw.
 *
 * The camp writes two shapes (Barton's screens, 2026-09-14). The full line
 * «Обучение этого отряда будет стоить 999 золотых монет» and the partial one
 * «За 100 золотых монет этот отряд получит 100 очков опыта». The client's
 * verdict the same day: the discount belongs to the price and the experience
 * stays vanilla. So the number to scale is the run stuck to the word
 * «золотых», and in the partial shape the experience run sits far later,
 * before «очков», where a digits-only walk from «золотых» can never reach it.
 *
 * It lives in a header with no game types so the contract test can compile and
 * run it on the host against those exact strings.
 */

#ifndef TRAINERLABELTEXT_H
#define TRAINERLABELTEXT_H

#include <cstddef>

namespace hooks {
namespace TrainerLabelText {

struct GoldRun
{
    const char* begin = nullptr;
    std::size_t len = 0;
};

inline bool isSpaceByte(char c)
{
    return c == ' ' || static_cast<unsigned char>(c) == 0xA0;
}

inline bool isDigitByte(char c)
{
    return c >= '0' && c <= '9';
}

inline GoldRun goldRunBefore(const char* begin, const char* anchor)
{
    if (!begin || !anchor || anchor < begin) {
        return {};
    }
    const char* p = anchor;
    while (p > begin && isSpaceByte(p[-1])) {
        --p;
    }
    const char* const runEnd = p;
    while (p > begin && isDigitByte(p[-1])) {
        --p;
    }
    if (p == runEnd || runEnd - p > 6) {
        return {};
    }
    for (const char* q = p; q < runEnd; ++q) {
        if (*q != '0') {
            return GoldRun{p, static_cast<std::size_t>(runEnd - p)};
        }
    }
    return {};
}

} // namespace TrainerLabelText
} // namespace hooks

#endif // TRAINERLABELTEXT_H
