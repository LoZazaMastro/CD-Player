// CD Player - Streaming resampling and audio analysis.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

namespace cd
{
class Resampler
{
    std::vector<float> buffer_;
    uint64_t origin_ = 0, next_ = 0;

  public:
    void reset();
    std::vector<float> push(std::span<const uint8_t> pcm, bool final);
};
struct AudioFrame
{
    float left = 0, right = 0;
    uint64_t epoch = 0, position = 0;
};
class AudioRing
{
    static constexpr size_t capacity_ = 1 << 17;
    std::vector<AudioFrame> frames_{capacity_};
    std::atomic<size_t> head_{0}, tail_{0};

  public:
    size_t space() const;
    size_t available() const;
    bool write(std::span<const float> pcm, uint64_t epoch, uint64_t position);
    bool read(AudioFrame &, uint64_t epoch, bool advance);
};
class Analyzer
{
    std::array<float, 1024> input_{};
    size_t at_ = 0;
    std::array<std::atomic<float>, 32> bands_{};
    std::array<std::atomic<float>, 256> waveform_{};

  public:
    void feed(float left, float right);
    std::array<float, 32> bands() const;
    std::array<float, 256> waveform() const;
};
} // namespace cd
