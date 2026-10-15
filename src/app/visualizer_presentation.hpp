// CD Player - Quiet fullscreen presentation for OLED displays.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace cd
{
struct VisualizerPresentation
{
    float hints = 1, labels = 1, x = 0, y = 0;
    bool blank = false;

    static VisualizerPresentation sample(bool oled, float idle, double time, bool playing)
    {
        VisualizerPresentation result;
        result.hints = std::clamp((2.f - idle) / .25f, 0.f, 1.f);
        if (oled)
        {
            result.labels = result.hints;
            // Move the entire effect, including any of its stationary details.
            result.x = float(std::sin(time * .071) * 84);
            result.y = float(std::sin(time * .053) * 54);
            result.blank = !playing && idle >= 60;
        }
        return result;
    }
};
} // namespace cd
