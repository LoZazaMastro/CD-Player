// CD Player - Native PS5 application entry and lifecycle.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/screen.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"
#include "platform/ps5/cd_output.hpp"
#include "platform/ps5/elevation/elevation.hpp"
#include <cstdio>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int sceKernelConvertUtcToLocaltime(int64_t, int64_t *, void *, uint64_t *);
extern "C" int sceSystemServiceGetStatus(void *);
namespace
{
std::string app_root()
{
    struct stat st
    {
    };
    if (stat("/app0/assets/fonts", &st) == 0)
        return "/app0";
    return "/data/homebrew/PPSA98427";
}
std::string clock_text()
{
    int64_t utc = std::time(nullptr), local = utc;
    uint64_t summer = 0;
    sceKernelConvertUtcToLocaltime(utc, &local, nullptr, &summer);
    const int64_t day = (local % 86400 + 86400) % 86400;
    char text[16];
    std::snprintf(text, sizeof(text), "%02d:%02d", int(day / 3600), int(day / 60 % 60));
    return text;
}
} // namespace
int main()
{
    const auto elevation_status = elevation::request(elevation::Capability::filesystem);
    const std::string root = app_root();
    const std::string data = elevation_status == elevation::Status::ok ? "/data/cdplayer-PPSA98427"
                                                                       : "/download0/cdplayer";
    mkdir(data.c_str(), 0700);
    FILE *log = std::freopen((data + "/session.log").c_str(), "w", stdout);
    if (log)
        std::setvbuf(log, nullptr, _IOLBF, 8192);
    hui::sys::log("[CD] title PPSA98427; filesystem status %u; assets %s",
                  unsigned(elevation_status), root.c_str());
    hui::ps5::Display display;
    if (!display.open(3840, 2160, 120))
    {
        hui::sys::log("[CD] display unavailable");
        hui::sys::quit();
    }
    cd::Screen screen;
    if (!screen.init(root + "/assets"))
    {
        hui::sys::log("[CD] renderer unavailable");
        hui::sys::quit();
    }
    hui::ps5::Pad pad;
    pad.open();
    hui::InputTracker tracker;
    cd::Player player;
    hui::sys::log("[CD startup] player constructed");
    cd::Dsp dsp;
    hui::audio::Mixer mixer;
    hui::audio::SoundBank sounds;
    sounds.load(root + "/assets/audio/sfx");
    hui::sys::log("[CD startup] UI sounds loaded");
    mixer.set_bus_gain(hui::audio::Bus::ui, .16f);
    player.audio_preferences(dsp);
    player.start(data);
    hui::sys::log("[CD startup] player workers started");
    cd::Output output(player, dsp, mixer);
    const bool audio_ok = output.start();
    screen.audio_channels(audio_ok ? output.channels() : 0);
    int64_t previous = hui::sys::monotonic_us();
    bool splash = true;
    int64_t measurement_start = previous;
    unsigned rendered_frames = 0;
    int64_t longest_frame = 0;
    int64_t longest_draw = 0;
    for (;;)
    {
        const int64_t now = hui::sys::monotonic_us();
        const float dt = float(now - previous) / 1000000.f;
        longest_frame = std::max(longest_frame, now - previous);
        previous = now;
        std::array<hui::PadSample, 64> samples;
        const size_t count = pad.read(samples);
        const auto input = tracker.update(std::span(samples).first(count), uint64_t(now));
        hui::ui::Feedback feedback;
        screen.handle(input, feedback, player, dsp);
        for (const auto &cue : feedback.cues)
            sounds.play(mixer, hui::audio::SoundSet::glass, cue);
        const auto state = player.snapshot();
        screen.update(dt, state, dsp);
        const int64_t draw_start = hui::sys::monotonic_us();
        screen.draw(state, player.spectrum(), dsp, clock_text(), display.width(), display.height(),0,player.waveform());
        longest_draw = std::max(longest_draw, hui::sys::monotonic_us()-draw_start);
        if (!display.swap())
            break;
        if (splash)
        {
            hui::sys::hide_splash_screen();
            splash = false;
            hui::sys::log("[CD] display accepted %dx%d @ %d Hz", display.width(), display.height(),
                          display.refresh());
        }
        if (input.focus_lost && state.playing)
            player.toggle_pause();
        // Hold Options and Circle for a clean exit; screenshot/Share remains owned by the system.
        if (input.is_held(hui::Action::menu) && input.is_pressed(hui::Action::back))
            break;
        ++rendered_frames;
        if (now - measurement_start >= 10000000)
        {
            hui::sys::log("[CD] rendering %.2f fps; longest frame %.2f ms; output %d Hz; audio "
                          "underruns %llu; draw max %.2f ms; visualizer %u; OLED %d",
                          double(rendered_frames) * 1000000 / double(now - measurement_start),
                          double(longest_frame) / 1000, display.refresh(),
                          (unsigned long long)state.underruns, double(longest_draw)/1000,
                          state.visualization, state.oled_visualizer);
            rendered_frames = 0;
            longest_frame = 0;
            longest_draw = 0;
            measurement_start = now;
            std::fflush(stdout);
        }
    }
    player.stop();
    output.stop();
    pad.close();
    std::fflush(nullptr);
    hui::sys::quit();
    return 0;
}
