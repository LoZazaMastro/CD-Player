// Adapted from iwrzwr visual archive by Kagan Yaldizkaya (2026, MIT).
// Native audio signal and renderer integration: Copyright (C) 2026 Andrea.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gfx/draw_list.hpp"
#include "app/archive_studies.hpp"
#include "app/archive_catalog.hpp"
#include <array>

namespace cd
{
class Visualizer
{
    struct Signal
    {
        float low = 0, mid = 0, high = 0, level = 0;
    };
    std::array<Signal, 240> history_{};
    Signal signal_{};
    size_t head_ = 0, count_ = 0;
    float history_clock_ = 0, motion_ = 0, low_phase_ = 0;
    float archive_time_ = 0;
    float peak_low_ = 0, peak_mid_ = 0, hit_age_ = 30, snare_age_ = 30;
    unsigned beat_ = 0;
    ArchiveStudies archive_;
    std::array<float,32> bands_{};
    float past(unsigned band, float age) const;
    void orbital(hui::gfx::DrawList &, float y, int study) const;
    void assembly(hui::gfx::DrawList &, float y, int study) const;
    void mechanics(hui::gfx::DrawList &, float y, int study) const;

  public:
    void update(const std::array<float, 32> &, float dt, bool playing, bool reduced_motion);
    void assets(const std::string &path) { archive_.assets(path); }
    void waveform(const std::array<float,256> &value) { archive_.waveform(value); }
    static std::string name(unsigned);
    static std::vector<std::string> names();
    void draw(hui::gfx::DrawList &, int selection, bool particles, const hui::ui::Fonts &);
};
} // namespace cd
