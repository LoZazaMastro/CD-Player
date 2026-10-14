// CD Player - Transport symbols on a fixed 15 x 15 dot grid.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gfx/draw_list.hpp"
#include <algorithm>
#include <cmath>

namespace cd
{
enum class DotIcon
{
    previous,
    play,
    pause,
    next,
    visualizer,
    settings,
    eject,
    reset
};
inline void draw_icon(hui::gfx::DrawList &out, const hui::gfx::Rect &box, DotIcon symbol,
                      hui::gfx::Color color)
{
    const float pitch = std::min(box.w, box.h) / 16;
    for (int row = 0; row < 15; ++row)
        for (int col = 0; col < 15; ++col)
        {
            const int x = col - 7, y = row - 7, ax = std::abs(x), ay = std::abs(y);
            bool on = false;
            switch (symbol)
            {
            case DotIcon::play:
                on = x >= -4 && x <= 5 && ay <= (5 - x) / 2;
                break;
            case DotIcon::pause:
                on = ay <= 5 && (ax == 2 || ax == 3);
                break;
            case DotIcon::previous:
                on = (x == -5 && ay <= 5) || (x >= -3 && x <= 4 && ay <= (x + 3) / 2);
                break;
            case DotIcon::next:
                on = (x == 5 && ay <= 5) || (x >= -4 && x <= 3 && ay <= (3 - x) / 2);
                break;
            case DotIcon::eject:
                on = (y == 4 && ax <= 5) || (y >= -5 && y <= 1 && ax <= (y + 5));
                break;
            case DotIcon::visualizer:
                on = ax <= 6 && ay <= (col % 3 == 0 ? 1 : col % 3 == 1 ? 4 : 6);
                break;
            case DotIcon::reset:
            {
                // A broad open loop with a separated, left-turning arrowhead.
                constexpr const char *arrow[] = {
                    "...............", ".......####....", ".....##...##...",
                    ".#..##.....##..", ".##.#.......##.", ".###........##.",
                    ".####........#.", ".#####.......#.", ".............#.",
                    ".............#.", "..##........##.", "...##......##..",
                    "....##....##...", ".....######....", "..............."};
                on = arrow[row][col] == '#';
                break;
            }
            case DotIcon::settings:
            {
                const int r = x * x + y * y;
                on = (r >= 16 && r <= 28) || (ay <= 1 && ax >= 4 && ax <= 6) ||
                     (ax <= 1 && ay >= 4 && ay <= 6) || (ax == ay && ax >= 3 && ax <= 4);
                break;
            }
            }
            if (on)
                out.circle(box.cx() + x * pitch, box.cy() + y * pitch, pitch * .30f, color);
        }
}
} // namespace cd
