// Geometry adapted from iwrzwr: orbital-memory.js, signal-assembly.js,
// phase-mechanics.js. Copyright (c) 2026 Kagan Yaldizkaya, MIT.
// Native adaptation: Copyright (C) 2026 Andrea.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/visualizer.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace cd
{
namespace
{
using hui::gfx::Color;
using hui::gfx::DrawList;
constexpr float tau = 2 * std::numbers::pi_v<float>;
constexpr float left = 300, width = 1320, scale = width / 412;
float fraction(float value)
{
    return value - std::floor(value);
}
float hash(float x, float y = 0)
{
    return fraction(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f);
}
Color ink(float alpha, float hot = 0)
{
    return hui::gfx::mix(Color::rgb(0xe4e4e4, alpha), Color::rgb(0xd71920, alpha), hot);
}
void line(DrawList &out, float y, float x1, float y1, float x2, float y2, float alpha,
          float hot = 0)
{
    out.line(left + x1 * scale, y + y1 * scale, left + x2 * scale, y + y2 * scale, 1.8f,
             ink(alpha, hot));
}
void mark(DrawList &out, float y, float x, float v, float alpha, float hot = 0)
{
    out.gradient_rect({left + x * scale, y + v * scale, 2 * scale, 2 * scale}, .4f, ink(alpha, hot),
                      ink(alpha * .72f, hot));
}
} // namespace

void Visualizer::update(const std::array<float, 32> &bands, float dt, bool playing,
                        bool reduced_motion)
{
    dt = std::clamp(dt, 0.f, .05f);
    for (unsigned i=0;i<bands.size();++i)
        bands_[i]=playing && std::isfinite(bands[i])?std::clamp(bands[i],0.f,1.f):0;
    Signal target;
    if (playing)
    {
        for (unsigned i = 0; i < bands.size(); ++i)
        {
            const float value = std::isfinite(bands[i]) ? std::clamp(bands[i], 0.f, 1.f) : 0;
            if (i < 13)
                target.low = std::max(target.low, value);
            else if (i < 25)
                target.mid = std::max(target.mid, value);
            else
                target.high = std::max(target.high, value);
            target.level += value / 32;
        }
    }
    target.level = std::clamp(target.level * 3, 0.f, 1.f);
    const float follow = 1 - std::exp(-dt * 12);
    auto smooth = [follow](float &a, float b) { a += (b - a) * follow; };
    smooth(signal_.low, target.low);
    smooth(signal_.mid, target.mid);
    smooth(signal_.high, target.high);
    smooth(signal_.level, target.level);
    hit_age_ += dt;
    snare_age_ += dt;
    if (playing && target.low > .08f && target.low > peak_low_ * 1.22f && hit_age_ > .18f)
    {
        hit_age_ = 0;
        ++beat_;
    }
    if (playing && target.mid > .06f && target.mid > peak_mid_ * 1.3f && snare_age_ > .13f)
        snare_age_ = 0;
    peak_low_ = std::max(target.low, peak_low_ * std::exp(-dt * 2.5f));
    peak_mid_ = std::max(target.mid, peak_mid_ * std::exp(-dt * 3));
    if (playing && !reduced_motion)
    {
        motion_ += dt * (.25f + signal_.level * 1.4f + std::exp(-hit_age_ * 9) * .4f);
        low_phase_ += dt * (.22f + signal_.low * .8f);
    }
    history_clock_ += dt;
    while (history_clock_ >= 1.f / 60)
    {
        history_clock_ -= 1.f / 60;
        head_ = (head_ + 1) % history_.size();
        history_[head_] = signal_;
        count_ = std::min(count_ + 1, history_.size());
    }
}

float Visualizer::past(unsigned band, float age) const
{
    const size_t back = size_t(std::max(0.f, age) * 60);
    if (back >= count_)
        return 0;
    const Signal &s = history_[(head_ + history_.size() - back) % history_.size()];
    return band == 0 ? s.low : band == 1 ? s.mid : band == 2 ? s.high : s.level;
}

void Visualizer::orbital(DrawList &out, float y, int study) const
{
    constexpr int cols = 102, rows = 10;
    for (int c = 0; c < cols; ++c)
        for (int r = 0; r < rows; ++r)
        {
            const float u = float(c) / (cols - 1) * 2 - 1, v = float(r) / 9 * 2 - 1;
            float level = 0, hot = 0;
            if (study == 0)
            {
                if ((c * 13 + r * 23 + (c ^ r) * 7) % 19 < 3)
                    continue;
                for (int i = 0; i < 5; ++i)
                {
                    const float age = std::min(2.1f, snare_age_ + i * .19f);
                    const float radius = .09f + age * .29f;
                    const float shell =
                        std::abs(std::hypot((u - (i - 2) * .37f) * 1.7f, v * .38f) - radius);
                    if (shell < .07f)
                    {
                        level = std::max(level, (.16f + past(1, i * .19f)) * std::exp(-age * .9f));
                        if (i == 0 && age < 1.3f && shell < .026f)
                            hot = .9f;
                    }
                }
            }
            else if (study == 1)
            {
                const float ax = std::cos(low_phase_) * (.25f + signal_.low * .45f);
                const float ay = std::sin(low_phase_) * (.25f + signal_.high * .5f);
                const float da = std::hypot((u - ax) * 1.5f, v - ay);
                const float db = std::hypot((u + ax) * 1.5f, v + ay);
                const float ring = std::hypot(u / .72f, v / .78f);
                hot = da < .16f || db < .16f ? .95f : 0;
                if (!hot && !(std::abs(ring - 1) < .11f && (c + r) % 3 == 0) &&
                    !((da < .48f || db < .48f) && (c + r) % 2 == 0))
                    continue;
                level = hash(float(c), float(7 + r)) > .88f ? 1 : .32f;
                if (hot)
                    level = .8f;
            }
            else
            {
                const int x = std::abs(c - cols / 2), vrow = std::abs(r - 4), band = x + vrow * 2;
                const float radius =
                    5 + std::max(signal_.low, std::exp(-hit_age_ * 9)) * (cols / 2);
                if (((x ^ (vrow * 3) ^ int(beat_)) % 7) >= 3 || band >= radius + 6)
                    continue;
                level = .35f + .4f * (1 - float(vrow) / 5);
                hot = std::abs(band - radius) < 2 ? .9f : 0;
            }
            if (level > .02f)
                mark(out, y, 3 + c * 4.f, 3 + r * 4.f, level, hot);
        }
}

void Visualizer::assembly(DrawList &out, float y, int study) const
{
    if (study == 0)
    {
        const float window = .24f, writer = .5f - .5f * std::cos(motion_ * tau / 20);
        const float start = writer * (1 - window), end = start + window;
        line(out, y, 8, 16, 404, 16, .32f);
        line(out, y, 8, 38, 404, 38, .24f);
        for (int i = 0; i < 17; ++i)
        {
            const float position = (i + .35f + hash(float(i)) * .25f) / 17;
            const float amplitude = .1f + past(3, (16 - i) * .13f) * .9f;
            const bool selected = position >= start && position <= end;
            const float x = 8 + position * 396;
            line(out, y, x, 38 - amplitude * 2, x, 38 + amplitude * 2, selected ? 1 : .4f,
                 selected ? .9f : 0);
            const float detail = 8 + (position - start * .5f) / window * 396;
            if (detail < 20 || detail > 392)
                continue;
            line(out, y, detail - 12, 16, detail, 16 - amplitude * 9, .9f);
            line(out, y, detail, 16 - amplitude * 9, detail + 5, 16 + amplitude * 7, .9f);
            line(out, y, detail + 5, 16 + amplitude * 7, detail + 12, 16, .9f);
        }
        const float a = 8 + start * 396, b = 8 + end * 396;
        line(out, y, a, 33, b, 33, .85f, .9f);
        line(out, y, a, 43, b, 43, .85f, .9f);
        line(out, y, a, 33, a, 43, .85f, .9f);
        line(out, y, b, 33, b, 43, .85f, .9f);
    }
    else if (study == 1)
    {
        const float shift = (signal_.high - signal_.low) * 18;
        line(out, y, 6, 17, 406, 17, .3f);
        line(out, y, 6, 27, 406, 27, .3f);
        for (int i = -25; i <= 25; ++i)
        {
            const float x = 206 + i * 8.f;
            const bool near = std::abs(std::remainder(x - 206 - shift, 9.f)) < 1;
            line(out, y, x, i % 5 == 0 ? 5 : 10, x, 17, near ? .95f : .45f, near ? .9f : 0);
            const float b = 206 + i * 9.f + shift;
            if (b < 6 || b > 406)
                continue;
            const bool aligned = std::abs(std::remainder(b - 206, 8.f)) < 1;
            line(out, y, b, 27, b, i % 5 == 0 ? 39 : 34, aligned ? .95f : .45f, aligned ? .9f : 0);
        }
        line(out, y, 206, 22, 206 + shift * .61f, 22, .95f, .9f);
    }
    else
    {
        constexpr float middle = 206;
        for (int lane = 0; lane < 3; ++lane)
        {
            const float a = 10 + lane * 12.f, b = 10 + ((lane + 1) % 3) * 12.f;
            auto at = [a, b](float x)
            { return a + (b - a) * std::clamp((x - 196) / 20, 0.f, 1.f); };
            line(out, y, 8, a, middle - 10, a, .16f);
            line(out, y, middle - 10, a, middle + 10, b, .16f);
            line(out, y, middle + 10, b, 404, b, .16f);
            const float travel = std::fmod(motion_ * 12, 24);
            for (int j = 0; j < 18; ++j)
            {
                const float x = 8 + j * 24.f - travel - lane * 8;
                const float len = 7 + past(unsigned(lane), std::abs(j - 8) * .06f) * 11;
                if (x < 8 || x + len > 404)
                    continue;
                for (int k = 0; k < 6; ++k)
                {
                    const float u = x + k / 6.f * len, v = x + (k + 1) / 6.f * len;
                    line(out, y, u, at(u), v, at(v), .75f,
                         x < middle && x + len > middle ? .9f : 0);
                }
            }
        }
    }
}

void Visualizer::mechanics(DrawList &out, float y, int study) const
{
    if (study == 0)
    {
        const float opening = 1.2f + std::max(signal_.low, std::exp(-hit_age_ * 9)) * 6;
        line(out, y, 4, 22, 82, 22, .85f);
        for (int i = 0; i < 5; ++i)
        {
            const float offset = float(i - 2), end = 8 + i * 7.f;
            line(out, y, 82, 22, 156, 22 + offset * .7f, .66f, i == 2 ? .9f : 0);
            line(out, y, 156, 22 + offset * .7f, 313, 22 + offset * opening, .66f,
                 i == 2 ? .9f : 0);
            line(out, y, 313, 22 + offset * opening, 403, end, .66f, i == 2 ? .9f : 0);
        }
    }
    else if (study == 1)
    {
        const float travel = fraction(motion_ / 4), pull = (signal_.high - signal_.low) * .9f;
        auto point = [travel, pull](float u, float v)
        {
            const float a = (u - travel) / .2f, b = (v - .5f) / .42f;
            const float influence =
                std::exp(-(a * a + b * b)) * .86f * std::sin(u * tau / 2) * std::sin(v * tau / 2);
            return std::array<float, 2>{5 + (u + (travel - u) * .38f * influence) * 402,
                                        5 + (v + pull * .32f * influence) * 34};
        };
        for (int row = 1; row < 4; ++row)
            for (int j = 0; j < 42; ++j)
            {
                const auto a = point(j / 42.f, row / 4.f), b = point((j + 1) / 42.f, row / 4.f);
                line(out, y, a[0], a[1], b[0], b[1], row == 2 ? 1.f : .31f);
            }
        for (int col = 1; col < 8; ++col)
            for (int j = 0; j < 20; ++j)
            {
                const auto a = point(col / 8.f, j / 20.f), b = point(col / 8.f, (j + 1) / 20.f);
                line(out, y, a[0], a[1], b[0], b[1], .32f);
            }
        const auto p = point(travel, .5f);
        line(out, y, p[0] - 4, p[1], p[0] + 4, p[1], .95f, .9f);
        line(out, y, p[0], p[1] - 4, p[0], p[1] + 4, .95f, .9f);
    }
    else
    {
        auto curve = [this](float x)
        { return .7f * (past(1, x * .35f) - .5f) + .25f * past(0, x * .35f) * std::sin(x * 5.5f); };
        const float cut = fraction(motion_ * .4675f / 2.4f);
        for (int rail = 0; rail < 5; ++rail)
        {
            const float z = rail / 4.f;
            for (int j = 0; j < 40; ++j)
            {
                const float x1 = j / 40.f, x2 = (j + 1) / 40.f;
                line(out, y, 10 + x1 * 382 + z * 10, 26 - curve(x1) * 10 - z * 12,
                     10 + x2 * 382 + z * 10, 26 - curve(x2) * 10 - z * 12, .32f);
            }
            mark(out, y, 10 + cut * 382 + z * 10, 26 - curve(cut) * 10 - z * 12, .9f, .9f);
        }
    }
}

std::string Visualizer::name(unsigned index)
{
    if(index>=1 && index<=archive_entries.size())return std::string(archive_entries[index-1].title);
    static const char* extra[]={"Echo Orchard (Orbital)","Twin Wells (Orbital)","Parity Bloom (Orbital)","Slice Loupe (Assembly)","Delay Vernier (Assembly)","Splice Loom (Assembly)","Section Gate (Mechanics)","Field Strain (Mechanics)","Phase Aperture (Mechanics)"};
    return extra[std::min(8u,index>archive_entries.size()?index-unsigned(archive_entries.size())-1:0u)];
}
std::vector<std::string> Visualizer::names(){std::vector<std::string> result;for(unsigned i=1;i<=visualizer_count;i++)result.push_back(name(i));return result;}
void Visualizer::draw(DrawList &out, int selection, bool particles, const hui::ui::Fonts &fonts)
{
    if(selection<=int(archive_entries.size()))
    {
        archive_.draw(out,unsigned(selection),motion_,signal_.low,signal_.mid,signal_.high,signal_.level,beat_,hit_age_,snare_age_,bands_,fonts);
    }
    else {
        const int item=selection-int(archive_entries.size())-1,family=item/3,study=item%3;
        if(family==0)orbital(out,440,study);
        else if(family==1)assembly(out,440,study);
        else mechanics(out,440,study);
    }
    if (particles)
        for (int i = 0; i < 44; ++i)
        {
            const float a = i * 2.399963f + motion_ * .02f, depth = 4 + hash(float(i)) * 4;
            const float x = 960 + std::cos(a) * (700 + signal_.low * 45) / depth * 5;
            const float y = 440 + std::sin(a) * 370 / depth * 4;
            const float alpha = .015f + .055f * signal_.level;
            out.circle(x, y, 1, ink(alpha));
        }
}
} // namespace cd
