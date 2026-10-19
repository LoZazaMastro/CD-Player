// CD Player - Album, track list, transport and listening preferences.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cd/player.hpp"
#include "cd/dsp.hpp"
#include "app/scene.hpp"
#include "app/visualizer.hpp"
#include "app/media_motion.hpp"
#include "app/spindle.hpp"
#include "gfx/renderer.hpp"
#include "ui/components/list.hpp"
#include "ui/components/button_group.hpp"
#include "ui/components/form.hpp"
#include "ui/glyphs.hpp"

namespace cd
{
class Screen
{
    friend struct ScreenInspection;
    hui::gfx::Renderer renderer_;
    Scene scene_;
    Visualizer visualizer_;
    MediaMotion media_motion_;
    Spindle spindle_;
    hui::gfx::Font regular_, semibold_, doto_, visual_mono_;
    hui::ui::Fonts fonts_;
    hui::ui::Theme theme_;
    hui::ui::ListView tracks_;
    hui::ui::Form preferences_;
    hui::tween::Spring settings_amount_;
    hui::tween::Spring album_amount_, cover_focus_amount_, album_scroll_motion_;
    hui::ui::ButtonGroup actions_;
    hui::gfx::DrawList page_, overlay_;
    GLuint cover_ = 0, pending_cover_ = 0;
    uint64_t revision_ = UINT64_MAX, cover_revision_ = UINT64_MAX;
    std::string shown_id_;
    float last_dt_ = 0, idle_ = 0;
    bool showing_playback_ = false;
    float time_ = 0;
    bool full_ = false, settings_ = false, toolbar_ = false, reduced_motion_ = false;
    bool gapless_ = true, show_eject_button_ = true;
    bool cover_focus_ = false, album_panel_ = false, album_available_ = false;
    float album_scroll_ = 0, album_scroll_max_ = 0;
    unsigned audio_channels_ = 8, playing_track_ = 0;
    unsigned observed_track_ = 0;
    bool equalizer_page_ = false;
    int eq_focus_ = 0;
    std::array<float, 10> eq_positions_{};
    size_t edition_count_ = SIZE_MAX;
    int visual_ = 1;
    bool lyrics_page_ = false;
    std::shared_ptr<const Lyrics> shown_lyrics_;
    struct LyricBlock
    {
        std::vector<std::string> rows;
        float center = 0, height = 0;
    };
    std::vector<LyricBlock> lyric_blocks_;
    hui::tween::Spring lyric_scroll_;
    int current_lyric_ = -1;
    float plain_scroll_ = 0;
    void update_lyrics(float dt, const Snapshot &);
    void draw_lyrics(const Snapshot &, const std::string &clock);
    bool load_font(const std::string &, hui::gfx::Font &, hui::ui::FontRef &);
    void refresh_preferences(const Snapshot &, Dsp &);

  public:
    ~Screen();
    bool init(const std::string &assets);
    void update(float dt, const Snapshot &, Dsp &);
    void handle(const hui::InputFrame &, hui::ui::Feedback &, Player &, Dsp &);
    void draw(const Snapshot &, const std::array<float, 32> &spectrum, Dsp &,
              const std::string &clock, int width, int height, GLuint framebuffer = 0,
              const std::array<float,256> &waveform = {});
    void preview_fullscreen(bool value)
    {
        full_ = value;
        lyrics_page_ = false;
        if (value)
            idle_ = 0;
    }
    void preview_lyrics(bool value)
    {
        full_ = value;
        lyrics_page_ = value;
        idle_ = 0;
    }
    void preview_preferences(bool value)
    {
        settings_ = value;
    }
    void preview_equalizer(bool value)
    {
        equalizer_page_ = value;
        settings_ = false;
    }
    void preview_album_panel(bool value) { album_panel_ = value; cover_focus_ = value; }
    void audio_channels(unsigned count)
    {
        audio_channels_ = count;
    }
};
} // namespace cd
