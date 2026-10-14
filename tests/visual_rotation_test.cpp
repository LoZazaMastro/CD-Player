// CD Player - Shuffle coverage and track-transition semantics.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/visual_rotation.hpp"
#include <cassert>
#include <cstdio>
#include <set>
int main()
{
    for (unsigned seed = 0; seed < 100; ++seed)
    {
        cd::VisualRotation rotation;
        rotation.seed(seed);
        unsigned track = 1, current = 1;
        assert(rotation.observe(track, current, true) == current);
        for (unsigned cycle = 0; cycle < 4; ++cycle)
        {
            std::set<unsigned> visited;
            for (unsigned i = 0; i < cd::visualizer_count; ++i)
            {
                track = track == 1 ? 2 : 1;
                const unsigned next = rotation.observe(track, current, true);
                assert(next >= 1 && next <= cd::visualizer_count && next != current);
                assert(visited.insert(next).second);
                current = next;
                assert(rotation.observe(track, current, true) == current);
                assert(rotation.observe(0, current, true) == current);
            }
        }
        rotation.reset_track();
        assert(rotation.observe(1, current, true) == current);
        assert(rotation.observe(2, current, false) == current);
        assert(rotation.observe(2, current, true) == current);
        assert(rotation.observe(1, current, true) != current);
        // Repeated manual choices must not break the no-repeat rule.
        for (unsigned i = 0; i < 1000; ++i)
        {
            track = track == 1 ? 2 : 1;
            current = i % cd::visualizer_count + 1;
            assert(rotation.observe(track, current, true) != current);
        }
    }
    std::puts("Shuffle passed: all 161 effects, no immediate repeat, pause/off/disc reset and "
              "manual selection");
}
