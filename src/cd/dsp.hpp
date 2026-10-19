// CD Player - Ten-band equalizer and live stereo-to-surround matrix.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <span>

namespace cd
{
constexpr std::array<float, 10> eq_frequencies = {31,   63,   125,  250,  500,
                                                  1000, 2000, 4000, 8000, 16000};
class Dsp
{
    struct Filter
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        std::array<double, 2> z1{}, z2{};
        float gain = 0;
        double process(double x, size_t channel);
        void tune(float db, float hz);
    };
    std::array<std::atomic<float>, 10> requested_{};
    std::array<Filter, 10> filters_{};
    std::atomic<bool> enabled_{false}, surround_{false};
    std::atomic<float> requested_surround_gain_{1}, requested_lfe_gain_{1};
    std::array<float, 4096> delay_left_{}, delay_right_{};
    size_t delay_at_ = 0;
    float surround_blend_ = 0, eq_blend_ = 0, lfe_ = 0;
    float surround_gain_ = 1, lfe_gain_ = 1;

  public:
    static constexpr float maximum_gain = 15.848932f; // +24 dB
    void equalizer(bool enabled)
    {
        enabled_.store(enabled);
    }
    bool equalizer() const
    {
        return enabled_.load();
    }
    void upmix(bool enabled)
    {
        surround_.store(enabled);
    }
    bool upmix() const
    {
        return surround_.load();
    }
    void band(size_t i, float db);
    void surround_gain(float value);
    void lfe_gain(float value);
    float surround_gain() const { return requested_surround_gain_.load(); }
    float lfe_gain() const { return requested_lfe_gain_.load(); }
    float band(size_t i) const;
    void process(std::span<const int16_t> stereo, std::span<int16_t> out, unsigned channels);
};
} // namespace cd
