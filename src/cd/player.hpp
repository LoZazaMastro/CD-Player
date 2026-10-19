// CD Player - Disc session and asynchronous playback control.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "app/archive_catalog.hpp"
#include "app/accent_palette.hpp"
#include "cd/disc.hpp"
#include "cd/sound.hpp"
#include "cd/dsp.hpp"
#include "cd/lyrics.hpp"
#include "cd/artwork.hpp"
#include "cd/visual_rotation.hpp"
#include <atomic>
#include <algorithm>
#include <mutex>
#include <pthread.h>

namespace cd
{
enum class State
{
    searching,
    empty,
    loading,
    ready,
    ejecting,
    error
};
struct Snapshot
{
    State state = State::searching;
    Disc disc;
    std::string drive, message = "Connect a USB CD drive";
    unsigned track = 0;
    double seconds = 0;
    bool playing = false;
    bool gapless = true, reduced_motion = false, visual_rotation = false, autoplay = false;
    bool show_eject_button = true;
    bool oled_visualizer = false;
    unsigned visualization = 1;
    unsigned accent_color = 0;
    float volume = 1;
    uint64_t revision = 0, cover_revision = 0, underruns = 0;
    std::shared_ptr<const std::vector<uint8_t>> cover;
    std::shared_ptr<const ArtworkImage> disc_art, back_art, spine_art;
    int cover_width = 0, cover_height = 0;
    std::vector<Release> editions;
    AlbumInfo album_info;
    bool album_info_ready = false, album_info_error = false;
    LyricsState lyrics_state = LyricsState::idle;
    std::shared_ptr<const Lyrics> lyrics;
};
class Player
{
    std::mutex mutex_;
    Snapshot snapshot_;
    std::atomic<bool> stop_{false}, playing_{false}, playback_started_{false};
    std::atomic<uint64_t> epoch_{1}, position_{0}, underruns_{0}, end_epoch_{0}, finished_epoch_{0};
    std::atomic<float> volume_{1.f};
    std::atomic<bool> gapless_{true}, reduced_motion_{false}, visual_rotation_{false}, autoplay_{false};
    std::atomic<bool> show_eject_button_{true};
    std::atomic<bool> oled_visualizer_{false};
    std::atomic<unsigned> visualization_{1};
    std::atomic<unsigned> accent_color_{0};
    VisualRotation rotation_;
    std::array<std::atomic<uint64_t>, 100> track_starts_{};
    AudioFrame held_frame_{};
    bool have_held_frame_ = false;
    unsigned gap_frames_ = 0;
    uint64_t previous_audio_position_ = UINT64_MAX, previous_audio_epoch_ = 0,
             last_gap_ = UINT64_MAX;
    bool started_ = false, metadata_started_ = false, lyrics_started_ = false, artwork_started_ = false;
    pthread_t io_thread_{}, metadata_thread_{}, lyrics_thread_{};
    pthread_t artwork_thread_{};
    std::atomic<bool> lyrics_visible_{false};
    std::string lyrics_key_;
    std::string lyrics_disc_, lyrics_title_, lyrics_artist_, lyrics_album_;
    uint32_t lyrics_duration_ = 0;
    unsigned lyrics_track_ = 0;
    std::string cache_;
    Dsp *dsp_ = nullptr;
    AudioRing ring_;
    Analyzer analyzer_;
    uint64_t seek_request_ = 0, seek_epoch_ = 0;
    bool eject_request_ = false;
    std::string metadata_id_;
    size_t edition_request_ = 0;
    uint64_t metadata_generation_ = 0;
    uint64_t artwork_generation_ = 0;
    void io_loop();
    void metadata_loop();
    void lyrics_loop();
    void artwork_loop();
    void set_state(State, std::string);
    static void *io_entry(void *);
    static void *metadata_entry(void *);
    static void *lyrics_entry(void *);
    static void *artwork_entry(void *);

  public:
    ~Player();
    bool start(std::string cache);
    void audio_preferences(Dsp &dsp)
    {
        dsp_ = &dsp;
    }
    void stop();
    Snapshot snapshot();
    void play_track(size_t index);
    void toggle_pause();
    void seek(double delta_seconds);
    void next(int direction);
    void eject_disc();
    void choose_edition(size_t);
    void lyrics_visible(bool value) { lyrics_visible_.store(value); }
    void volume(float v)
    {
        volume_.store(v);
    }
    float volume() const
    {
        return volume_.load();
    }
    void gapless(bool value)
    {
        gapless_.store(value);
    }
    bool gapless() const
    {
        return gapless_.load();
    }
    void reduced_motion(bool value)
    {
        reduced_motion_.store(value);
    }
    void visualization(unsigned value)
    {
        visualization_.store(std::clamp(value, 1u, visualizer_count));
    }
    void show_eject_button(bool value) { show_eject_button_.store(value); }
    void oled_visualizer(bool value) { oled_visualizer_.store(value); }
    void accent_color(unsigned value) { accent_color_.store(std::min(value, unsigned(accent_colors.size()-1))); }
    void autoplay(bool value) { autoplay_.store(value); }
    void visual_rotation(bool value)
    {
        visual_rotation_.store(value);
    }
    void render(int16_t *samples, unsigned frames);
    std::array<float, 32> spectrum() const
    {
        return analyzer_.bands();
    }
    std::array<float, 256> waveform() const
    {
        return analyzer_.waveform();
    }
    size_t buffered_frames() const
    {
        return ring_.available();
    }
};
} // namespace cd
