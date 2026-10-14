// CD Player - Original iwrzwr drawing programs rendered through Prospero UI.
// Copyright (C) 2026 Andrea.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gfx/draw_list.hpp"
#include "ui/fonts.hpp"
#include <array>
#include <memory>
#include <string>
namespace cd {
class ArchiveStudies {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    ArchiveStudies();
    ~ArchiveStudies();
    void assets(const std::string &);
    void waveform(const std::array<float,256> &);
    bool draw(hui::gfx::DrawList &, unsigned selection, float time, float low, float mid, float high,
              float level, unsigned beat, float kick_age, float snare_age,
              const std::array<float,32> &, const hui::ui::Fonts &);
    const std::string &error() const;
};
}
