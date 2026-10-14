// CD Player - Native stereo and eight-channel AudioOut sink.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cd/player.hpp"
#include "cd/dsp.hpp"
#include "audio/mixer.hpp"
namespace cd
{
class Output
{
    Player &player_;
    Dsp &dsp_;
    hui::audio::Mixer &cues_;
    int handle_ = -1;
    unsigned channels_ = 8;
    pthread_t thread_{};
    bool started_ = false;
    std::atomic<bool> stop_{false};
    static void *entry(void *);
    void loop();

  public:
    Output(Player &p, Dsp &d, hui::audio::Mixer &c) : player_(p), dsp_(d), cues_(c)
    {
    }
    ~Output();
    bool start();
    void stop();
    unsigned channels() const
    {
        return channels_;
    }
};
} // namespace cd
