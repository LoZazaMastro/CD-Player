// CD Player - Continuous 44.1 to 48 kHz conversion and spectrum analysis.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/sound.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace cd
{
void Resampler::reset()
{
    buffer_.clear();
    origin_ = next_ = 0;
}
std::vector<float> Resampler::push(std::span<const uint8_t> pcm, bool final)
{
    for (size_t i = 0; i + 3 < pcm.size(); i += 4)
    {
        const int16_t left = int16_t(uint16_t(pcm[i]) | uint16_t(pcm[i + 1]) << 8);
        const int16_t right = int16_t(uint16_t(pcm[i + 2]) | uint16_t(pcm[i + 3]) << 8);
        buffer_.push_back(float(left) / 32768.f);
        buffer_.push_back(float(right) / 32768.f);
    }
    const uint64_t end = origin_ + buffer_.size() / 2;
    std::vector<float> out;
    constexpr int radius = 16;
    while (next_ * 147 / 160 < end)
    {
        const double source = double(next_) * 147. / 160.;
        const int64_t center = int64_t(source);
        if (!final && uint64_t(center + radius) >= end)
            break;
        double sums[2] = {}, norm = 0;
        for (int tap = -radius + 1; tap <= radius; ++tap)
        {
            const int64_t frame = center + tap;
            const double x = source - double(frame);
            const double sinc =
                std::abs(x) < 1e-10 ? 1. : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
            const double window = 0.42 + 0.5 * std::cos(std::numbers::pi * x / radius) +
                                  0.08 * std::cos(2. * std::numbers::pi * x / radius);
            const double weight = sinc * window;
            norm += weight;
            if (frame >= int64_t(origin_) && frame < int64_t(end))
            {
                const size_t i = size_t(frame - int64_t(origin_)) * 2;
                sums[0] += buffer_[i] * weight;
                sums[1] += buffer_[i + 1] * weight;
            }
        }
        out.push_back(float(sums[0] / norm));
        out.push_back(float(sums[1] / norm));
        ++next_;
    }
    const uint64_t next_source = next_ * 147 / 160;
    const uint64_t keep_from = next_source > radius ? next_source - radius : 0;
    if (keep_from > origin_)
    {
        const size_t drop = size_t(std::min(keep_from - origin_, uint64_t(buffer_.size() / 2)));
        buffer_.erase(buffer_.begin(), buffer_.begin() + ptrdiff_t(drop * 2));
        origin_ += drop;
    }
    return out;
}
size_t AudioRing::available() const
{
    return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
}
size_t AudioRing::space() const
{
    return capacity_ - available();
}
bool AudioRing::write(std::span<const float> pcm, uint64_t epoch, uint64_t position)
{
    const size_t count = pcm.size() / 2;
    const size_t head = head_.load(std::memory_order_relaxed);
    if (count > capacity_ - (head - tail_.load(std::memory_order_acquire)))
        return false;
    for (size_t i = 0; i < count; ++i)
        frames_[(head + i) % capacity_] = {pcm[i * 2], pcm[i * 2 + 1], epoch, position + i};
    head_.store(head + count, std::memory_order_release);
    return true;
}
bool AudioRing::read(AudioFrame &frame, uint64_t epoch, bool advance)
{
    size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t head = head_.load(std::memory_order_acquire);
    while (tail != head && frames_[tail % capacity_].epoch != epoch)
        ++tail;
    tail_.store(tail, std::memory_order_release);
    if (tail == head || !advance)
        return false;
    frame = frames_[tail % capacity_];
    tail_.store(tail + 1, std::memory_order_release);
    return true;
}
void Analyzer::feed(float left, float right)
{
    input_[at_++] = (left + right) * .5f;
    if (at_ != input_.size())
        return;
    at_ = 0;
    for (size_t i=0;i<waveform_.size();++i)
        waveform_[i].store(input_[i*4],std::memory_order_relaxed);
    std::array<std::complex<float>, 1024> values;
    for (unsigned i = 0; i < 1024; ++i)
    {
        unsigned rev = 0, n = i;
        for (unsigned b = 0; b < 10; ++b)
        {
            rev = (rev << 1) | (n & 1);
            n >>= 1;
        }
        values[rev] = {
            input_[i] * (.5f - .5f * std::cos(2.f * std::numbers::pi_v<float> * i / 1023.f)), 0};
    }
    for (unsigned step = 2; step <= 1024; step *= 2)
    {
        const auto phase = std::polar(1.f, -2.f * std::numbers::pi_v<float> / step);
        for (unsigned start = 0; start < 1024; start += step)
        {
            std::complex<float> w = 1.f;
            for (unsigned i = 0; i < step / 2; ++i)
            {
                const auto a = values[start + i], b = values[start + i + step / 2] * w;
                values[start + i] = a + b;
                values[start + i + step / 2] = a - b;
                w *= phase;
            }
        }
    }
    for (unsigned band = 0; band < 32; ++band)
    {
        const unsigned first = unsigned(std::pow(512., double(band) / 32));
        const unsigned last =
            std::min(511u, std::max(first + 1, unsigned(std::pow(512., double(band + 1) / 32))));
        float peak = 0;
        for (unsigned i = first; i < last; ++i)
            peak = std::max(peak, std::abs(values[i]) / 100.f);
        const float old = bands_[band].load(std::memory_order_relaxed);
        bands_[band].store(std::clamp(std::max(peak, old * .78f), 0.f, 1.f),
                           std::memory_order_relaxed);
    }
}
std::array<float, 32> Analyzer::bands() const
{
    std::array<float, 32> out{};
    for (size_t i = 0; i < 32; ++i)
        out[i] = bands_[i].load(std::memory_order_relaxed);
    return out;
}
std::array<float, 256> Analyzer::waveform() const
{
    std::array<float,256> out{};
    for(size_t i=0;i<out.size();++i)out[i]=waveform_[i].load(std::memory_order_relaxed);
    return out;
}
} // namespace cd
