// CD Player - Interrupted media transitions and frame-rate independent rotation.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/media_motion.hpp"
#include "app/spindle.hpp"
#include <cstdio>
#include <cstdlib>

namespace
{
void require(bool value, const char *message)
{
    if (!value)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}
} // namespace
int main()
{
    cd::MediaMotion media;
    float previous = 0;
    for (int i = 0; i < 120; ++i)
    {
        media.update(true, false, 1.f / 120, false);
        require(media.disc() >= previous && media.disc() <= 1, "disc insertion monotonic");
        require(media.cover() == 0, "artwork absent during initial TOC read");
        previous = media.disc();
    }
    require(media.disc() == 1, "disc settles");
    for (int i = 0; i < 120; ++i)
        media.update(true, true, 1.f / 120, false);
    require(media.cover() == 1, "late metadata artwork appears");
    for (int i = 0; i < 120; ++i)
    {
        media.update(false, false, 1.f / 120, false);
        if (media.cover() > .001f)
            require(media.disc() == 1, "cover departs before disc");
    }
    for (int i = 0; i < 120; ++i)
        media.update(false, false, 1.f / 120, false);
    require(media.disc() == 0 && media.cover() == 0, "removal finishes");
    for (int i = 0; i < 15; ++i)
        media.update(true, false, 1.f / 120, false);
    const float partial = media.disc();
    require(partial > 0 && partial < 1, "partial insertion");
    media.update(false, false, 1.f / 120, false);
    require(media.disc() <= partial && media.disc() > 0, "removal reverses from visible position");
    media.update(true, false, 1.f / 120, false);
    require(media.disc() > 0 && media.disc() < 1, "reinsertion has no jump");
    cd::Spindle thirty, sixty, one_twenty;
    for (int phase = 0; phase < 2; ++phase)
    {
        for (int i = 0; i < 30; ++i)
            thirty.update(1. / 30, phase == 0, false);
        for (int i = 0; i < 60; ++i)
            sixty.update(1. / 60, phase == 0, false);
        for (int i = 0; i < 120; ++i)
            one_twenty.update(1. / 120, phase == 0, false);
        require(std::abs(thirty.angle() - one_twenty.angle()) < .000001,
                "same angle at 30 and 120 fps");
        require(std::abs(sixty.angle() - one_twenty.angle()) < .000001,
                "same angle at 60 and 120 fps");
    }
    cd::Spindle disc;
    disc.update(1. / 120, true, false);
    require(disc.speed() > 0 && disc.speed() < .54, "spin-up begins gently");
    for (int i = 0; i < 180; ++i)
        disc.update(1. / 120, true, false);
    const double speed = disc.speed();
    disc.update(1. / 120, false, false);
    require(disc.speed() > 0 && disc.speed() < speed, "pause coasts rather than snapping");
    for (int i = 0; i < 720; ++i)
        disc.update(1. / 120, false, false);
    require(disc.speed() == 0, "rotation eventually stops");
    const float stopped = disc.angle();
    disc.update(1. / 120, false, false);
    require(disc.angle() == stopped, "stopped disc stays still");
    for (int i = 0; i < 600; ++i)
        disc.update(1. / 120, true, false);
    require(std::abs(disc.speed() - 18.) < .0001, "running speed is ten times the original 1.8 rad/s");
    for (int i = 0; i < 90; ++i)
    {
        disc.update(1. / 120, true, false, true);
        require(disc.angle() == 0 && disc.speed() == 0,
                "new insertion keeps engraving upright on the right, including during loading");
    }
    disc.update(1. / 120, true, false);
    require(disc.angle() < 0 && disc.speed() > 0 && disc.speed() < .54,
            "spin-up starts gradually in the reversed direction after entry settles");
    std::puts("Motion passed: ordered cover/disc removal, interrupted insertion, 30/60/120 fps "
              "equivalence, spin-up, coast-down");
}
