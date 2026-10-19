// CD Player - RBJ peaking filters with gain ramps and decorrelated ambience.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/dsp.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace cd
{
double Dsp::Filter::process(double x, size_t channel)
{
    const double y = b0 * x + z1[channel];
    z1[channel] = b1 * x - a1 * y + z2[channel];
    z2[channel] = b2 * x - a2 * y;
    return y;
}
void Dsp::Filter::tune(float db, float hz)
{
    gain = db;
    const double a = std::pow(10., db / 40.), w = 2. * std::numbers::pi * hz / 48000.;
    const double alpha = std::sin(w) / (2. * 1.41421356237), c = std::cos(w);
    const double divisor = 1. + alpha / a;
    b0 = (1. + alpha * a) / divisor;
    b1 = -2. * c / divisor;
    b2 = (1. - alpha * a) / divisor;
    a1 = -2. * c / divisor;
    a2 = (1. - alpha / a) / divisor;
}
void Dsp::band(size_t i, float db)
{
    if (i < 10)
        requested_[i].store(std::clamp(db, -12.f, 12.f));
}
float Dsp::band(size_t i) const
{
    return i < 10 ? requested_[i].load() : 0;
}
void Dsp::channel_gain(size_t channel, float value)
{
    if (channel < requested_gains_.size() && std::isfinite(value))
        requested_gains_[channel].store(std::clamp(value, 0.f, maximum_gain));
}
float Dsp::channel_gain(size_t channel) const
{
    return channel < requested_gains_.size() ? requested_gains_[channel].load() : 1.f;
}
void Dsp::process(std::span<const int16_t> stereo, std::span<int16_t> out, unsigned channels)
{
    if ((channels != 2 && channels != 8) || stereo.size() % 2 ||
        out.size() != stereo.size() / 2 * channels)
        return;
    bool flat = true;
    for (size_t band = 0; band < 10; ++band)
    {
        const float target = requested_[band].load();
        const float next =
            filters_[band].gain + std::clamp(target - filters_[band].gain, -.15f, .15f);
        if (next != filters_[band].gain)
            filters_[band].tune(next, eq_frequencies[band]);
        flat &= next == 0;
    }
    const bool eq = enabled_.load(), upmix = surround_.load() && channels == 8;
    std::array<float, 8> targets;
    for (size_t c = 0; c < targets.size(); ++c)
        targets[c] = requested_gains_[c].load();
    auto to_s16 = [](double sample)
    { return int16_t(std::clamp(std::lrint(sample * 32768.), -32768l, 32767l)); };
    for (size_t frame = 0; frame < stereo.size() / 2; ++frame)
    {
        const double left = double(stereo[frame * 2]) / 32768.,
                     right = double(stereo[frame * 2 + 1]) / 32768.;
        double l = left, r = right;
        for (auto &f : filters_)
        {
            l = f.process(l, 0);
            r = f.process(r, 1);
        }
        eq_blend_ += std::clamp((eq ? 1.f : 0.f) - eq_blend_, -1.f / 2400, 1.f / 2400);
        l = left + (l - left) * eq_blend_;
        r = right + (r - right) * eq_blend_;
        surround_blend_ +=
            std::clamp((upmix ? 1.f : 0.f) - surround_blend_, -1.f / 4800, 1.f / 4800);
        const float k = surround_blend_;
        std::array<double, 8> values = {
            l * (1. - .2 * k), r * (1. - .2 * k), .28 * k * (l + r), 0, 0, 0, 0, 0};
        lfe_ += .01558f * (float((l + r) * .5) - lfe_);
        values[3] = lfe_ * .22 * k;
        const size_t back = (delay_at_ + 4096 - 1200) % 4096,
                     side = (delay_at_ + 4096 - 720) % 4096;
        // The AudioOut 8-channel layout is FL, FR, FC, LFE, BL, BR, SL, SR.
        values[4] = delay_left_[back] * .3 * k;
        values[5] = delay_right_[back] * .3 * k;
        values[6] = delay_left_[side] * .36 * k;
        values[7] = delay_right_[side] * .36 * k;
        for (unsigned c = 0; c < channels; ++c)
        {
            gains_[c] += std::clamp(targets[c] - gains_[c], -maximum_gain/4800, maximum_gain/4800);
            values[c] *= gains_[c];
            // Preserve the original front signal at unity. Only a deliberately
            // boosted front channel uses the same peak protection as the upmix.
            if ((c >= 2 || gains_[c] > 1.f) && std::abs(values[c]) > .9)
                values[c] = std::copysign(.9 + .1 * std::tanh((std::abs(values[c])-.9)/.1), values[c]);
        }
        delay_left_[delay_at_] = float(l - r);
        delay_right_[delay_at_] = float(r - l);
        delay_at_ = (delay_at_ + 1) % 4096;
        for (unsigned c = 0; c < channels; ++c)
            out[frame * channels + c] = to_s16(values[c]);
        if ((!eq || flat) && eq_blend_ == 0 && k == 0)
        {
            if (gains_[0] == 1.f) out[frame * channels] = stereo[frame * 2];
            if (gains_[1] == 1.f) out[frame * channels + 1] = stereo[frame * 2 + 1];
        }
    }
}
} // namespace cd
