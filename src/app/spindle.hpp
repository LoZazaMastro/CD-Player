// CD Player - Frame-rate independent spin-up and coast-down.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace cd
{
class Spindle
{
    double angle_ = 0, velocity_ = 0;

  public:
    void update(double dt, bool running, bool reduced_motion, bool entering = false)
    {
        dt = std::clamp(dt, 0., .1);
        if (entering)
        {
            // Keep the engraving horizontal on the right throughout entry;
            // spin-up starts only once the disc has reached its position.
            angle_ = 0;
            velocity_ = 0;
            return;
        }
        if (reduced_motion)
        {
            velocity_ = 0;
            return;
        }
        const double target = running ? 18. : 0.;
        const double tau = running ? .38 : .48;
        const double decay = std::exp(-dt / tau);
        angle_ -= target * dt + (velocity_ - target) * tau * (1 - decay);
        angle_ = std::fmod(angle_, 6.283185307179586);
        velocity_ = target + (velocity_ - target) * decay;
        if (!running && velocity_ < .002)
            velocity_ = 0;
    }
    float angle() const
    {
        return float(angle_);
    }
    double speed() const
    {
        return velocity_;
    }
};
} // namespace cd
