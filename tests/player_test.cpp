// CD Player - Whole-session regression with a controlled MMC drive.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/player.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
namespace
{
std::atomic<bool> medium{true};
std::atomic<unsigned> wakes{0}, ejections{0}, reads{0};
std::atomic<bool> lyrics_enabled{false}, lyrics_busy{false};
std::atomic<unsigned> lyric_requests{0};
std::atomic<unsigned> transient_lyrics_failures{0};
std::atomic<unsigned> artwork_requests{0};
class FakeDrive : public cd::Drive
{
  public:
    std::string label() const override
    {
        return "Apple SuperDrive fixture";
    }
    cd::Reply execute(const cd::Command &c, std::span<uint8_t> out) override
    {
        cd::Reply r;
        r.ok = true;
        r.transferred = c.allocation;
        switch (c.bytes[0])
        {
        case 0x12:
        {
            std::fill(out.begin(), out.end(), 0);
            out[0] = 5;
            const std::string s = "Apple   SuperDrive      2.00";
            std::copy(s.begin(), s.end(), out.begin() + 8);
            r.transferred = 36;
            break;
        }
        case 0xea:
            ++wakes;
            break;
        case 0:
            if (!medium.load())
            {
                r.ok = false;
                r.sense_key = 2;
                r.asc = 0x3a;
            }
            break;
        case 0x43:
        {
            if (c.bytes[2] == 5)
            {
                r.ok = false;
                r.sense_key = 5;
                break;
            }
            std::fill(out.begin(), out.end(), 0);
            out[1] = 26;
            out[2] = 1;
            out[3] = 2;
            out[6] = 1;
            out[14] = 2;
            out[19] = 16;
            out[22] = 170;
            out[27] = 32;
            r.transferred = 28;
            break;
        }
        case 0xbe:
        {
            ++reads;
            const unsigned count = c.bytes[8];
            if (!medium.load())
            {
                r.ok = false;
                r.sense_key = 2;
                r.asc = 0x3a;
                break;
            }
            for (size_t i = 0; i < size_t(count) * 2352; i += 4)
            {
                out[i] = 0xd2;
                out[i + 1] = 0x04;
                out[i + 2] = 0xd2;
                out[i + 3] = 0x04;
            }
            break;
        }
        case 0x1e:
            break;
        case 0x1b:
            ++ejections;
            medium.store(false);
            break;
        default:
            r.ok = false;
            break;
        }
        return r;
    }
};
void require(bool value, const char *message)
{
    if (!value)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::abort();
    }
}
void wait_state(cd::Player &p, cd::State state)
{
    for (unsigned i = 0; i < 300; ++i)
    {
        if (p.snapshot().state == state)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    require(false, "session state timeout");
}
void wait_buffer(cd::Player &p)
{
    for (unsigned i = 0; i < 500; ++i)
    {
        if (p.buffered_frames() >= 20480)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    require(false, "prefill timeout");
}
} // namespace
namespace cd
{
std::vector<std::string> drive_names()
{
    return {"fixture"};
}
std::unique_ptr<Drive> open_drive(const std::string &, std::string &)
{
    return std::make_unique<FakeDrive>();
}
int http_get(const std::string &url, std::vector<uint8_t> &out, size_t limit)
{
    if(lyrics_enabled.load() && url=="https://coverartarchive.org/release/12345678-1234-1234-1234-123456789abc/")
    {
        const std::string body=R"({"images":[{"id":100,"types":["Medium"],"approved":true},{"id":101,"types":["Back","Spine"],"approved":true}]})";
        out.assign(body.begin(),body.end());++artwork_requests;return 200;
    }
    if(lyrics_enabled.load() && (url.ends_with("/100-1200.jpg") || url.ends_with("/101-1200.jpg")))
    {
        std::ifstream fixture("tests/fixtures/artwork-tray.png",std::ios::binary);
        out.assign(std::istreambuf_iterator<char>(fixture),{});
        require(out.size()<=limit,"artwork response bounded");++artwork_requests;return 200;
    }
    if(lyrics_enabled.load() && url.starts_with("https://lrclib.net/"))
    {
        ++lyric_requests;lyrics_busy.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if(transient_lyrics_failures.exchange(0)) {lyrics_busy.store(false);return 503;}
        const bool second=url.find("Fixture%20Two")!=std::string::npos;
        const std::string response=std::string(R"({"trackName":")")+(second?"Fixture Two":"Fixture One")+
            R"(","artistName":"Fixture Artist","duration":0.213333,"syncedLyrics":"[00:00.00]A test phrase\n[00:00.10]Another test phrase"})";
        require(response.size()<=limit,"lyrics response bounded");
        out.assign(response.begin(),response.end());lyrics_busy.store(false);return 200;
    }
    return 404;
}
} // namespace cd
int main()
{
    std::filesystem::create_directories("build/player-test-data");
    std::filesystem::remove("build/player-test-data/settings.json");
    cd::Dsp preferences;
    cd::Player p;
    p.audio_preferences(preferences);
    require(p.start("build/player-test-data"), "worker start");
    wait_state(p, cd::State::ready);
    require(wakes == 1, "Apple wake once");
    require(p.snapshot().show_eject_button, "Eject button visible by default");
    require(!p.snapshot().autoplay && !p.snapshot().playing, "autoplay defaults off; inserted disc waits for playback");
    require(p.snapshot().disc.tracks.size() == 2, "two audio tracks");
    require(!p.snapshot().visual_rotation, "visualizer rotation defaults off");
    p.visual_rotation(true);
    const unsigned original_visualizer = p.snapshot().visualization;
    p.toggle_pause();
    wait_buffer(p);
    std::vector<int16_t> grain(1024);
    p.render(grain.data(), 512);
    p.toggle_pause();
    const auto paused = p.snapshot().seconds;
    p.render(grain.data(), 512);
    require(p.snapshot().seconds == paused, "pause retains position");
    require(p.snapshot().visualization == original_visualizer, "pause cannot rotate visualizers");
    require(std::all_of(grain.begin(), grain.end(), [](auto s) { return s == 0; }), "pause silent");
    p.toggle_pause();
    std::vector<int16_t> around(18000 * 2);
    p.render(around.data(), 18000);
    const unsigned second_visualizer = p.snapshot().visualization;
    require(second_visualizer != original_visualizer, "audible next track rotates visualizer");
    require(p.snapshot().visualization == second_visualizer,
            "polling same track cannot rotate twice");
    p.visual_rotation(false);
    for (size_t i = 9500; i < 11500; ++i)
        if (around[i * 2] <= 1000)
        {
            std::fprintf(stderr,
                         "boundary frame %zu sample %d; starts %d/%d/%d; buffered %zu, playing %d, "
                         "underruns %llu\n",
                         i, int(around[i * 2]), int(around[0]), int(around[9500 * 2]),
                         int(around[11500 * 2]), p.buffered_frames(), int(p.snapshot().playing),
                         (unsigned long long)p.snapshot().underruns);
            require(false, "gapless boundary has no injected silence");
        }
    p.gapless(false);
    p.play_track(0);
    require(p.snapshot().visualization == second_visualizer,
            "disabled rotation retains chosen effect");
    p.toggle_pause();
    p.render(grain.data(), 512);
    wait_buffer(p);
    p.toggle_pause();
    std::vector<int16_t> gap(70000 * 2);
    p.render(gap.data(), 70000);
    size_t quiet = 0;
    for (size_t i = 10240; i < 58240; ++i)
        if (gap[i * 2] == 0)
            ++quiet;
    require(quiet == 48000, "optional one-second gap is exact");
    require(gap[58240 * 2] > 1000, "first frame after gap preserved");
    require(!p.snapshot().playing, "end of buffered disc stops playback");
    p.eject_disc();
    wait_state(p, cd::State::empty);
    require(ejections == 1, "one physical eject command");
    p.render(grain.data(), 512);
    require(std::all_of(grain.begin(), grain.end(), [](auto s) { return s == 0; }),
            "no old audio after eject");
    p.autoplay(true);
    medium.store(true);
    wait_state(p, cd::State::ready);
    require(p.snapshot().playing && p.snapshot().track == 1, "autoplay starts first audio track after insertion");
    p.toggle_pause();
    require(!p.snapshot().playing, "autoplay disc can be paused normally");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    require(!p.snapshot().playing, "disc polling never restarts paused autoplay");
    p.eject_disc();
    wait_state(p, cd::State::empty);
    p.autoplay(false);
    medium.store(true);
    wait_state(p, cd::State::ready);
    require(!p.snapshot().playing, "autoplay off leaves reinserted disc stopped");
    p.autoplay(true);
    require(!p.snapshot().playing, "changing autoplay does not start already inserted disc");
    p.gapless(true);
    p.play_track(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    p.render(grain.data(), 512);
    require(p.snapshot().track == 2, "reinsertion and direct second-track playback");
    p.visual_rotation(true);
    p.show_eject_button(false);
    p.oled_visualizer(true);
    p.accent_color(24);
    p.volume(.37f);
    p.gapless(false);
    p.reduced_motion(true);
    preferences.equalizer(true);
    preferences.upmix(true);
    for(size_t c=0;c<8;++c) preferences.channel_gain(c,float(c));
    for(size_t band=0;band<10;++band)preferences.band(band,float(band)-5);
    p.visualization(147);
    bool saved = false;
    for (unsigned attempt = 0; attempt < 200; ++attempt)
    {
        std::ifstream file("build/player-test-data/settings.json");
        const std::string bytes((std::istreambuf_iterator<char>(file)), {});
        if (bytes.find("\"visualization\": 147") != std::string::npos)
        {
            saved = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    require(saved, "rotation toggle persisted");
    p.stop();
    cd::Player restored;
    cd::Dsp restored_preferences;
    restored.audio_preferences(restored_preferences);
    require(restored.start("build/player-test-data"), "restore worker start");
    const auto restored_snapshot = restored.snapshot();
    require(!restored_snapshot.show_eject_button, "Eject button visibility restored");
    require(restored_snapshot.oled_visualizer, "OLED mode restored");
    require(restored_snapshot.accent_color==24,"accent color restored");
    require(restored_snapshot.autoplay, "autoplay preference restored");
    require(restored_snapshot.visual_rotation, "rotation preference restored");
    require(restored_snapshot.visualization == 147, "manually selected visualizer restored");
    require(std::abs(restored.volume() - .37f) < .001f, "volume restored");
    require(!restored.gapless(), "gapless preference restored");
    require(restored_snapshot.reduced_motion, "motion preference restored");
    require(restored_preferences.equalizer() && restored_preferences.upmix(), "DSP toggles restored into fresh DSP");
    for(size_t c=0;c<8;++c)
        require(restored_preferences.channel_gain(c)==float(c),"every speaker gain restored independently");
    for(size_t band=0;band<10;++band)
        require(std::abs(restored_preferences.band(band)-(float(band)-5)) < .001f, "every EQ band restored");
    restored.stop();
    std::filesystem::create_directories("build/player-legacy-test-data");
    {std::ofstream legacy("build/player-legacy-test-data/settings.json");legacy<<R"({"version":1,"surround_gain":4,"lfe_gain":0,"upmix":true})";}
    cd::Player legacy_player;cd::Dsp legacy_dsp;legacy_player.audio_preferences(legacy_dsp);
    require(legacy_player.start("build/player-legacy-test-data"),"legacy preferences load");
    require(legacy_player.snapshot().accent_color==0,"legacy preferences retain original accent");
    for(size_t c=0;c<8;++c)
        require(legacy_dsp.channel_gain(c)==(c<2?1:c==3?0:4),"legacy gains migrate without changing fronts or losing mute");
    legacy_player.stop();
    const std::string lyric_cache="build/player-lyrics-test-data";
    std::filesystem::create_directories(lyric_cache);
    for(const auto &file:std::filesystem::directory_iterator(lyric_cache))
        if(file.is_regular_file() && (file.path().filename().string().starts_with("lyrics-") ||
           file.path().filename().string().starts_with("12345678-")))
            std::filesystem::remove(file.path());
    {
        std::ofstream metadata(lyric_cache+"/Eoi6K6eMdKcHaqJ9LHWKtlPgElI-.json");
        metadata<<R"({"releases":[{"id":"12345678-1234-1234-1234-123456789abc","title":"Fixture Album","artist-credit":[{"name":"Fixture Artist"}],"media":[{"discs":[{"id":"Eoi6K6eMdKcHaqJ9LHWKtlPgElI-"}],"tracks":[{"position":1,"title":"Fixture One"},{"position":2,"title":"Fixture Two"}]}]}]})";
    }
    cd::Player lyric_player;cd::Dsp lyric_dsp;lyric_player.audio_preferences(lyric_dsp);
    lyrics_enabled.store(true);
    require(lyric_player.start(lyric_cache),"lyrics workers start");wait_state(lyric_player,cd::State::ready);
    for(unsigned n=0;n<300 && lyric_player.snapshot().disc.artist.empty();++n)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(lyric_player.snapshot().disc.artist=="Fixture Artist","lyrics use resolved album metadata");
    for(unsigned n=0;n<300 && !lyric_player.snapshot().spine_art;++n)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto artwork=lyric_player.snapshot();
    require(artwork.disc_art && artwork.back_art && artwork.spine_art,"background worker loads all three physical surfaces");
    require(artwork.disc_art->width==64 && artwork.back_art->width==72 && artwork.spine_art->width==4,
            "worker uses independent disc/back/spine preparation");
    require(artwork_requests.load()==3,"combined back/spine scan is downloaded once per recognized release");
    require(lyric_requests.load()==0,"no lyrics request until page is opened");
    transient_lyrics_failures.store(1);
    lyric_player.lyrics_visible(true);
    for(unsigned n=0;n<200 && lyric_requests.load()==0;++n)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    require(lyric_player.snapshot().lyrics_state==cd::LyricsState::loading,"503 remains in loading state during automatic retry");
    const auto wait_lyrics=[&](){for(unsigned n=0;n<400;++n){const auto s=lyric_player.snapshot();if(s.lyrics_state==cd::LyricsState::ready && s.lyrics)return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}require(false,"lyrics timeout");};
    wait_lyrics();require(lyric_player.snapshot().lyrics->synced && lyric_requests.load()==2,"503 recovers automatically without toggling R3");
    const auto held_lyrics=lyric_player.snapshot().lyrics;
    for(unsigned reopening=0;reopening<3;++reopening)
    {
        lyric_player.lyrics_visible(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        lyric_player.lyrics_visible(true);
        for(unsigned tick=0;tick<20;++tick)
        {
            const auto shown=lyric_player.snapshot();
            require(shown.lyrics_state==cd::LyricsState::ready && shown.lyrics==held_lyrics,
                    "R3 reopen reuses ready lyrics with no loading flash or reparse");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    require(lyric_requests.load()==2,"reopening lyrics makes no new network requests");
    lyric_player.play_track(1);
    for(unsigned n=0;n<200 && !lyrics_busy.load();++n)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    require(lyrics_busy.load(),"second-track request in progress");
    lyric_player.play_track(0);
    require(!lyric_player.snapshot().lyrics,"late track response cannot display against a different audible track");
    wait_lyrics();
    require(lyric_player.snapshot().track==1 && lyric_requests.load()==3,"return to earlier track uses cache");
    lyric_player.lyrics_visible(false);lyric_player.stop();lyrics_enabled.store(false);
    std::printf("Session passed: wake, TOC, pause, exact gap/gapless, eject, reinsertion, track "
                "selection (%u reads)\n",
                reads.load());
}
