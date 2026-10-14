// CD Player - Ordered insertion, cover reveal and removal transitions.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/tween.hpp"
#include <algorithm>
#include <cmath>

namespace cd
{
class MediaMotion
{
    class Ramp
    {
        float from_ = 0, target_ = 0, elapsed_ = 1;

      public:
        float value = 0;
        void step(bool visible, float dt, float duration, bool reduced)
        {
            const float next = visible ? 1.f : 0.f;
            if (target_ != next)
            {
                from_ = value;
                target_ = next;
                elapsed_ = 0;
            }
            if (reduced)
            {
                value = next;
                return;
            }
            elapsed_ = std::min(1.f, elapsed_ + dt / duration);
            const float t = elapsed_;
            const float eased = visible ? hui::tween::cubic_out(t) : hui::tween::cubic_in(t);
            value = from_ + (target_ - from_) * eased;
        }
    };
    Ramp disc_, cover_;

  public:
    void update(bool medium, bool artwork, float dt, bool reduced)
    {
        dt = std::clamp(dt, 0.f, .05f);
        cover_.step(medium && artwork && disc_.value > .85f, dt, .48f, reduced);
        disc_.step(medium || cover_.value > .001f, dt, .72f, reduced);
    }
    float disc() const
    {
        return disc_.value;
    }
    float cover() const
    {
        return cover_.value;
    }
};
} // namespace cd
