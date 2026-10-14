// CD Player - Shuffle visualizers only when the audible track changes.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "app/archive_catalog.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace cd
{
class VisualRotation
{
    std::array<unsigned, visualizer_count> order_{};
    unsigned cursor_ = visualizer_count, track_ = 0;
    uint64_t random_ = 1;
    uint32_t random()
    {
        random_ ^= random_ >> 12;
        random_ ^= random_ << 25;
        random_ ^= random_ >> 27;
        return uint32_t((random_ * UINT64_C(2685821657736338717)) >> 32);
    }

  public:
    void seed(uint64_t value)
    {
        random_ = value ? value : 1;
    }
    void reset_track()
    {
        track_ = 0;
    }
    unsigned observe(unsigned track, unsigned current, bool enabled)
    {
        if (track == 0)
            return current; // End of disc is not another song.
        const bool changed = track_ != 0 && track_ != track;
        track_ = track;
        if (!enabled || !changed)
            return current;
        for (;;)
        {
            if (cursor_ == order_.size())
            {
                for (unsigned i = 0; i < order_.size(); ++i)
                    order_[i] = i + 1;
                for (unsigned i = unsigned(order_.size()) - 1; i > 0; --i)
                    std::swap(order_[i], order_[random() % (i + 1)]);
                cursor_ = 0;
            }
            // Manual visualizer selection can change the current entry too.
            if (order_[cursor_] == current)
            {
                if (cursor_ + 1 < order_.size())
                    std::swap(order_[cursor_], order_[cursor_ + 1]);
                else
                {
                    cursor_ = unsigned(order_.size());
                    continue;
                }
            }
            return order_[cursor_++];
        }
    }
};
} // namespace cd
