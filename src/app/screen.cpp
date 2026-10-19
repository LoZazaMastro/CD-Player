// CD Player - A dark listening room with a silver disc and warm focus light.

// Copyright (C) 2026 Andrea

// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/screen.hpp"

#include "app/dot_icons.hpp"
#include "app/visualizer_presentation.hpp"

#include "core/save_file.hpp"

#include <algorithm>

#include <cmath>

#include <cstdio>

namespace cd
{
using namespace hui;

namespace
{
const gfx::Color ink = gfx::Color::rgb(0xffffff), muted = gfx::Color::rgb(0x999999);

float gain_db(float gain)
{
    return gain > 0 ? std::clamp(20.f * std::log10(gain), -48.f, 24.f) : -49.f;
}
float db_gain(float db)
{
    return db <= -49 ? 0 : std::pow(10.f, db / 20.f);
}
std::string gain_label(float db)
{
    if (db <= -49) return "Mute";
    char value[20];
    std::snprintf(value, sizeof(value), "%+.0f dB", double(db));
    return value;
}

std::string duration(double seconds)
{
    const int total = std::max(0, int(seconds));
    char value[32];

    std::snprintf(value, sizeof(value), "%d:%02d", total / 60, total % 60);
    return value;
}

float centered_baseline(const gfx::Font &font, std::string_view text, float size, float center)
{
    std::vector<gfx::GlyphQuad> glyphs;
    font.layout(text, 0, 0, size, gfx::Align::left, glyphs);
    if (glyphs.empty())
        return center;
    float top = glyphs.front().y0, bottom = glyphs.front().y1;
    for (const auto &g : glyphs)
    {
        top = std::min(top, g.y0);
        bottom = std::max(bottom, g.y1);
    }
    return center - (top + bottom) * .5f;
}

} // namespace

Screen::~Screen()
{
    if (pending_cover_)
        glDeleteTextures(1, &pending_cover_);
    if (cover_)
        glDeleteTextures(1, &cover_);
}

void Screen::apply_accent(unsigned index)
{
    accent_index_ = std::min(index, unsigned(accent_colors.size()-1));
    accent = gfx::Color::rgb(accent_colors[accent_index_].rgb);
    theme_.primary = theme_.accent = theme_.focus = accent;
    tracks_.style.theme = actions_.style.theme = preferences_.style.theme = speakers_.style.theme = theme_;
}

ui::GlyphStyle Screen::dot_style() const
{
    auto style = ui::GlyphStyle::dots();
    style.label = accent;
    return style;
}

bool Screen::load_font(const std::string &path, gfx::Font &font, ui::FontRef &ref)
{
    std::string bytes;

    if (!save::read_file(path, &bytes) || !font.load(bytes))
        return false;

    ref = {&font, renderer_.batch().create_font_texture(font)};
    return true;
}

bool Screen::init(const std::string &assets)
{
    if (!renderer_.init() || !scene_.init(assets))
        return false;
    visualizer_.assets(assets);

    if (!load_font(assets + "/fonts/inter-regular.huifont", regular_, fonts_.regular) ||

        !load_font(assets + "/fonts/inter-semibold.huifont", semibold_, fonts_.semibold))
        return false;

    if (!load_font(assets + "/fonts/doto-semibold.huifont", doto_, fonts_.display))
        return false;
    doto_.add_fallback(&regular_, fonts_.regular.texture);
    fonts_.regular=fonts_.display;fonts_.semibold=fonts_.display;
    if (!load_font(assets + "/fonts/visualizer-mono.huifont", visual_mono_, fonts_.mono))
        return false;
    fonts_.pixel = fonts_.regular;
    fonts_.hand = fonts_.regular;

    theme_ = ui::themes().front();

    theme_.page = gfx::Color::rgb(0x000000);
    theme_.surface = gfx::Color::rgb(0x111111);

    theme_.surface_high = gfx::Color::rgb(0x222222);
    theme_.text = ink;
    theme_.text_muted = muted;

    theme_.primary = theme_.accent = theme_.focus = accent;
    theme_.on_primary = gfx::Color::rgb(0xffffff);

    theme_.secondary = gfx::Color::rgb(0x191919);
    theme_.on_secondary = ink;

    theme_.outline = gfx::Color::rgb(0x333333);
    theme_.style = ui::SurfaceStyle::flat;
    theme_.radius = 16;

    tracks_.style.theme = theme_;
    tracks_.style.row_height = 78;
    tracks_.style.gap = 6;

    tracks_.style.leading_width = 68;
    tracks_.style.subtitle_size = 21;
    tracks_.style.title_size = 27;

    tracks_.style.highlight.kind = ui::HighlightKind::bar;
    tracks_.style.highlight.bar_fill = 0;
    tracks_.style.highlight.thickness = 3;
    tracks_.style.dividers = true;

    tracks_.style.sounds.rumble = 0;
    tracks_.set_bounds({902, 417, 900, 514});

    tracks_.leading =
        [this](ui::Canvas &canvas, const gfx::Rect &row, const ui::ListItem &item, int, float)
    {
        if (playing_track_ != 0 && unsigned(item.tag) == playing_track_)
        {
            draw_icon(canvas.list, {row.x + 22, row.cy() - 16, 32, 32}, DotIcon::play, accent);
            return;
        }
        char number[16];
        std::snprintf(number, sizeof(number), "%02d", item.tag);
        ui::text(canvas.list, canvas.fonts.display, number, row.x + 24,
                 centered_baseline(*canvas.fonts.display.font, number, 26, row.cy()), 26, accent);
    };
    tracks_.trailing =
        [](ui::Canvas &canvas, const gfx::Rect &row, const ui::ListItem &item, int, float)
    {
        ui::text(canvas.list, canvas.fonts.display, item.value, row.x + row.w - 26,
                 centered_baseline(*canvas.fonts.display.font, item.value, 24, row.cy()), 24, muted,
                 gfx::Align::right);
    };
    actions_.style.theme = theme_;
    actions_.style.sounds.rumble = 0;

    actions_.style.height = 76;
    actions_.style.gap = 20;
    actions_.style.icon_width = 52;

    actions_.set_bounds({902, 222, 900, 76});

    actions_.set_items({{""}, {""}, {""}, {""}, {""}, {""}});

    actions_.icon = [this](ui::Canvas &canvas, const gfx::Rect &box, const ui::GroupItem &,
                           int index, gfx::Color color, float focus)
    {
        const DotIcon symbols[] = {
            DotIcon::previous, showing_playback_ ? DotIcon::pause : DotIcon::play,
            DotIcon::next,     DotIcon::visualizer,
            DotIcon::settings, DotIcon::eject};

        draw_icon(canvas.list, box, symbols[index], gfx::mix(color, accent, focus * .6f));
    };

    actions_.set_focus(1);
    actions_.set_active(false);

    preferences_.style.theme = theme_;
    preferences_.style.row_height = 78;
    preferences_.style.gap = 6;
    preferences_.style.header_height = 38;
    preferences_.style.label_size = 25;
    preferences_.style.value_size = 24;
    preferences_.style.number_width = 136;
    preferences_.style.header_size = 20;
    preferences_.style.sounds.rumble = 0;
    preferences_.style.toggle_text = false;
    preferences_.style.focus_values = true;
    preferences_.style.label_ratio = .5f;
    preferences_.style.description_inline = false;
    preferences_.style.entrance_step = 0;
    preferences_.style.on_page = false;
    preferences_.style.control_width = 342;
    preferences_.style.highlight.kind = ui::HighlightKind::ring;
    preferences_.set_bounds({1034, 186, 758, 732});
    speakers_.style = preferences_.style;
    speakers_.set_bounds({1034, 186, 758, 732});
    const size_t order[] = {0, 1, 2, 4, 5, 6, 7, 3};
    const char *names[] = {"Front left", "Front right", "Center", "Surround left",
                           "Surround right", "Side left", "Side right", "Subwoofer (LFE)"};
    for (size_t i = 0; i < 8; ++i)
    {
        auto &row = speakers_.add_slider(int(order[i]), names[i], 0, -49, 24, 1);
        row.format = gain_label;
        row.description = order[i] < 2 ? "Original front channel; 0 dB keeps its current level"
            : "Generated upmix channel; 0 dB keeps its original level";
    }
    return true;
}

void Screen::refresh_preferences(const Snapshot &snapshot, Dsp &dsp)
{
    if (edition_count_ != snapshot.editions.size())
    {
        edition_count_ = snapshot.editions.size();
        preferences_.clear();
        preferences_.add_header("Audio");
        auto &eq = preferences_.add_action(0, "Equalizer");
        eq.chevron = true;
        eq.text = "10 bands";
        preferences_.add_toggle(1, "Upmix 7.1", dsp.upmix()).description =
            "Surround derived from stereo audio";
        auto &speakers = preferences_.add_action(12, "Speaker setup");
        speakers.chevron = true;
        speakers.description = "Adjust the eight speaker levels independently";
        preferences_.add_toggle(2, "Gapless playback", gapless_);
        preferences_.add_toggle(8, "Autoplay disc", snapshot.autoplay);
        auto &volume = preferences_.add_slider(5, "Volume", snapshot.volume * 100, 0, 100, 5);
        volume.unit = " %";
        volume.description = "Right stick up/down adjusts volume";
        preferences_.add_header("Visualization");
        preferences_.add_toggle(3, "Reduced motion", reduced_motion_);
        preferences_.add_choice(4, "Visualizer", Visualizer::names(), visual_ - 1);
        preferences_.add_toggle(7, "Shuffle visualizers", snapshot.visual_rotation);
        preferences_.add_toggle(11, "OLED mode", snapshot.oled_visualizer).description =
            "Hide labels, gently move effects, and show a black screen after one minute paused";
        preferences_.add_header("Album");
        if (snapshot.editions.size() > 1)
        {
            std::vector<std::string> editions;
            for (const auto &r : snapshot.editions)
                editions.push_back(r.country + " · " + r.date);
            preferences_.add_choice(6, "Edition", std::move(editions));
        }
        else
            preferences_.add_value(6, "Edition",
                                   snapshot.editions.empty() ? "Not available"
                                                             : snapshot.editions.front().date);
        preferences_.add_header("Interface");
        std::vector<std::string> color_names;
        for (const auto &color : accent_colors) color_names.emplace_back(color.name);
        auto &color = preferences_.add_choice(14, "Accent color", std::move(color_names), int(accent_index_));
        color.choice.option = [](ui::Canvas &canvas, const gfx::Rect &area, int index, float)
        {
            const auto &color = accent_colors[size_t(index)];
            const float size = 24, swatch = 22, gap = 12;
            const float text_width = canvas.fonts.display.measure(color.name, size);
            const float x = area.cx()-(swatch+gap+text_width)*.5f;
            canvas.list.rounded_rect({x, area.cy()-swatch*.5f, swatch, swatch}, 0, gfx::Color::rgb(color.rgb));
            ui::text(canvas.list, canvas.fonts.display, color.name, x+swatch+gap,
                     area.cy()+size*.35f, size, ink);
        };
        preferences_.add_toggle(10, "Show Eject button", snapshot.show_eject_button);
    }
    preferences_.set_disabled(1, audio_channels_ != 8);
    preferences_.set_toggle(1, dsp.upmix());
    preferences_.set_toggle(2, gapless_);
    preferences_.set_toggle(3, reduced_motion_);
    preferences_.set_choice(4, visual_ - 1);
    preferences_.set_toggle(7, snapshot.visual_rotation);
    preferences_.set_toggle(8, snapshot.autoplay);
    preferences_.set_toggle(10, snapshot.show_eject_button);
    preferences_.set_toggle(11, snapshot.oled_visualizer);
    preferences_.set_choice(14, int(accent_index_));
    preferences_.set_slider(5, snapshot.volume * 100);
    preferences_.set_active(true);
}
void Screen::update(float dt, const Snapshot &snapshot, Dsp &dsp)
{
    last_dt_ = std::isfinite(dt) ? std::clamp(dt, 0.f, .25f) : 0;
    dt = std::min(last_dt_, .05f);
    volume_feedback_ = std::max(0.f, volume_feedback_-last_dt_);
    if (accent_index_ != snapshot.accent_color) apply_accent(snapshot.accent_color);
    settings_amount_.target = settings_ ? 1.f : 0.f;
    if (reduced_motion_)
        settings_amount_.snap(settings_amount_.target);
    else
        settings_amount_.update(dt, 16);
    for (size_t i = 0; i < 10; ++i)
        eq_positions_[i] += (dsp.band(i) - eq_positions_[i]) * (1 - std::exp(-dt * 22));
    time_ += last_dt_;
    idle_ += last_dt_;
    if (show_eject_button_ != snapshot.show_eject_button)
    {
        show_eject_button_ = snapshot.show_eject_button;
        const int focus = actions_.focus();
        actions_.set_items(std::vector<ui::GroupItem>(show_eject_button_ ? 6 : 5));
        actions_.set_focus(std::min(focus, show_eject_button_ ? 5 : 4));
    }
    showing_playback_ = snapshot.playing;
    playing_track_ = snapshot.playing ? snapshot.track : 0;
    album_available_ = bool(snapshot.cover) && !snapshot.album_info.annotation.empty();
    if (!album_available_)
    {
        cover_focus_ = false;
        album_panel_ = false;
    }
    album_amount_.target = album_panel_ ? 1.f : 0.f;
    cover_focus_amount_.target = cover_focus_ && album_available_ ? 1.f : 0.f;
    if (snapshot.reduced_motion)
    {
        album_amount_.snap(album_amount_.target);
        cover_focus_amount_.snap(cover_focus_amount_.target);
    }
    else
    {
        album_amount_.update(dt, 16);
        cover_focus_amount_.update(dt, 20);
    }

    album_scroll_motion_.target = std::clamp(album_scroll_, 0.f, album_scroll_max_);
    if (snapshot.reduced_motion)
        album_scroll_motion_.snap(album_scroll_motion_.target);
    else
        album_scroll_motion_.update(dt, 14);

    gapless_ = snapshot.gapless;
    reduced_motion_ = snapshot.reduced_motion;
    if (visual_ != int(snapshot.visualization))
        idle_ = 0;
    visual_ = int(snapshot.visualization);

    const bool medium = !snapshot.disc.id.empty() || snapshot.state == State::loading;
    spindle_.update(dt, snapshot.playing || snapshot.state == State::loading,
                    reduced_motion_, medium && media_motion_.disc() < .999f);
    if (snapshot.revision != revision_)
    {
        revision_ = snapshot.revision;

        if (snapshot.disc.id != shown_id_)
        {
            shown_id_ = snapshot.disc.id;
            observed_track_ = 0;
            tracks_.set_focus(0);
        }

        std::vector<ui::ListItem> items;

        for (const auto &track : snapshot.disc.tracks)
        {
            ui::ListItem item;
            item.title = track.title;
            item.subtitle = "";

            item.value = duration(double(track.end - track.start) / 75.);
            item.tag = int(track.number);
            item.disabled = !track.audio();

            items.push_back(std::move(item));
        }

        const int focus = tracks_.focus();
        tracks_.set_items(std::move(items));
        tracks_.set_focus(focus, false);
    }

    // Follow track transitions once; never pull the list back while the user browses.
    if (snapshot.track != observed_track_)
    {
        observed_track_ = snapshot.track;
        if (snapshot.state == State::ready && snapshot.track != 0)
            for (size_t i = 0; i < snapshot.disc.tracks.size(); ++i)
                if (snapshot.disc.tracks[i].number == snapshot.track)
                {
                    tracks_.reveal_item(int(i), snapshot.reduced_motion);
                    break;
                }
    }

    if (snapshot.cover_revision != cover_revision_)
    {
        cover_revision_ = snapshot.cover_revision;
        if (pending_cover_)
        {
            glDeleteTextures(1, &pending_cover_);
            pending_cover_ = 0;
        }
        if (snapshot.cover)
        {
            const GLuint next = renderer_.batch().create_texture(
                snapshot.cover_width, snapshot.cover_height, snapshot.cover->data());
            if (cover_)
                pending_cover_ = next;
            else
                cover_ = next;
        }
    }
    media_motion_.update(medium, snapshot.cover && cover_ && !pending_cover_, dt, reduced_motion_);
    if (media_motion_.cover() < .001f)
    {
        if (pending_cover_)
        {
            if (cover_)
                glDeleteTextures(1, &cover_);
            cover_ = pending_cover_;
            pending_cover_ = 0;
        }
        else if (!snapshot.cover && cover_)
        {
            glDeleteTextures(1, &cover_);
            cover_ = 0;
        }
    }
    if (settings_)
    {
        refresh_preferences(snapshot, dsp);
        preferences_.set_active(!speaker_page_);
        for (size_t c = 0; c < 8; ++c)
        {
            speakers_.set_slider(int(c), gain_db(dsp.channel_gain(c)));
            speakers_.set_disabled(int(c), c >= 2 && (audio_channels_ != 8 || !dsp.upmix()));
        }
    }
    speakers_.set_active(settings_ && speaker_page_);

    tracks_.set_active(!toolbar_ && !cover_focus_ && !album_panel_ && !full_ && !settings_ && !equalizer_page_);
    actions_.set_active(toolbar_ && !cover_focus_ && !album_panel_ && !full_ && !settings_ && !equalizer_page_);

    tracks_.update(dt);
    actions_.update(dt);
    preferences_.update(dt);
    speakers_.update(dt);
    if (lyrics_page_)
        update_lyrics(dt, snapshot);
}

void Screen::update_lyrics(float dt, const Snapshot &snapshot)
{
    const bool changed = shown_lyrics_ != snapshot.lyrics;
    if (changed)
    {
        shown_lyrics_ = snapshot.lyrics;
        lyric_blocks_.clear();
        plain_scroll_ = 0;
        float top = 0;
        if (shown_lyrics_)
            for (const auto &line : shown_lyrics_->lines)
            {
                LyricBlock block;
                block.rows = doto_.wrap(line.text, 58, 1480);
                if (block.rows.empty()) block.rows.emplace_back();
                block.height = float(block.rows.size()) * 80;
                block.center = top + block.height * .5f;
                top += block.height + 48;
                lyric_blocks_.push_back(std::move(block));
            }
    }
    current_lyric_ = shown_lyrics_ ? lyric_line_at(*shown_lyrics_, snapshot.seconds) : -1;
    const auto index = size_t(std::max(0, current_lyric_));
    lyric_scroll_.target = shown_lyrics_ && shown_lyrics_->synced && !lyric_blocks_.empty()
        ? lyric_blocks_[std::min(index, lyric_blocks_.size()-1)].center
        : plain_scroll_ + (lyric_blocks_.empty() ? 0.f : lyric_blocks_.front().center);
    if (changed || reduced_motion_) lyric_scroll_.snap(lyric_scroll_.target);
    else lyric_scroll_.update(dt, 12);
}

void Screen::draw_lyrics(const Snapshot &snapshot, const std::string &clock)
{
    const auto presentation = VisualizerPresentation::sample(
        snapshot.oled_visualizer, idle_, time_, snapshot.playing);
    if (presentation.blank) return;

    const float drift_x = snapshot.oled_visualizer ? presentation.x * .12f : 0;
    const float drift_y = snapshot.oled_visualizer ? presentation.y * .12f : 0;
    page_.push_clip({96, 200, 1728, 680});
    page_.push_transform(1, 0, 0, drift_x, drift_y);
    if (!lyric_blocks_.empty())
    {
        for (size_t i = 0; i < lyric_blocks_.size(); ++i)
        {
            const auto &block = lyric_blocks_[i];
            const float center = 540 + block.center - lyric_scroll_.value;
            if (center + block.height * .5f < 200 || center - block.height * .5f > 880) continue;
            const bool active = shown_lyrics_->synced && int(i) == current_lyric_;
            for (size_t row = 0; row < block.rows.size(); ++row)
            {
                const float cy = center + (float(row) - (float(block.rows.size())-1)*.5f)*80;
                const float edge = std::clamp(std::min(cy-200, 880-cy)/90, 0.f, 1.f);
                const float alpha = active ? 1.f : !shown_lyrics_->synced ? .8f : int(i)<current_lyric_ ? .25f : .5f;
                const auto color = (active ? accent : ink).with_alpha(edge * alpha);
                ui::text(page_, fonts_.display, block.rows[row], 960,
                    cy+(doto_.ascent(58)-doto_.descent(58))*.5f, 58, color, gfx::Align::center);
            }
        }
    }
    else
    {
        const char *message = snapshot.state != State::ready || snapshot.track == 0 ? "Play a track to view lyrics"
            : snapshot.lyrics_state == LyricsState::loading || snapshot.lyrics_state == LyricsState::idle ? "Finding lyrics..."
            : shown_lyrics_ && shown_lyrics_->instrumental ? "Instrumental"
            : "Lyrics unavailable for this track";
        ui::text(page_, fonts_.display, message, 960,
            centered_baseline(doto_, message, 38, 540), 38, muted, gfx::Align::center);
    }
    page_.pop_transform();
    page_.pop_clip();
    // Track labels share the visualizer's OLED behavior. Sung lines remain visible.
    if (presentation.labels > 0)
    {
        page_.push_opacity(presentation.labels);
        std::string title = snapshot.disc.album;
        for (const auto &track : snapshot.disc.tracks)
            if (track.number == snapshot.track) title = track.title;
        ui::text(page_, fonts_.display, doto_.fit(title, 32, 1400), 96, 118, 32, ink);
        ui::text(page_, fonts_.display, clock, 1824,
            centered_baseline(doto_, clock, 64.5f, 98), 64.5f, ink, gfx::Align::right);
        page_.pop_opacity();
    }
    if (presentation.hints > 0)
    {
        page_.push_opacity(presentation.hints);
        if (shown_lyrics_ && !shown_lyrics_->synced && !shown_lyrics_->instrumental)
            ui::text(page_, fonts_.display, "Synced lyrics unavailable", 960, 928, 24, muted, gfx::Align::center);
        const ui::Hint hints[] = {{ui::Button::circle, "Back"}, {ui::Button::cross, "Play / pause"},
                                 {ui::Button::r3, "Visualizer"}, {ui::Button::dpad, "Scroll"}};
        const int count = shown_lyrics_ && !shown_lyrics_->synced && !shown_lyrics_->instrumental ? 4 : 3;
        ui::draw_hints(page_, fonts_, dot_style(), hints, count, 96, false);
        page_.pop_opacity();
    }
}

void Screen::handle(const InputFrame &input, ui::Feedback &feedback, Player &player, Dsp &dsp)
{
    if (input.pressed || input.released || input.held || input.nav != Direction::none ||

        std::abs(input.stick_x) > .1f || std::abs(input.stick_y) > .1f ||

        std::abs(input.stick2_x) > .1f || std::abs(input.stick2_y) > .1f)
        idle_ = 0;

    if (std::isfinite(input.stick2_y) && std::abs(input.stick2_y) > .25f)
    {
        const float amount = std::copysign((std::abs(input.stick2_y)-.25f)/.75f, input.stick2_y);
        player.volume(std::clamp(player.volume()-amount*.35f*std::min(last_dt_,.05f), 0.f, 1.f));
        volume_feedback_ = 1.8f;
    }

    if (input.is_pressed(Action::north))
    {
        player.eject_disc();
        feedback.play(audio::Cue::back);
        return;
    }

    if (input.is_pressed(Action::west))
    {
        player.toggle_pause();
        feedback.play(audio::Cue::select);
        return;
    }

    if (input.is_pressed(Action::menu))
    {
        settings_ = !settings_;
        speaker_page_ = false;
        album_panel_ = false;
        full_ = false;
        lyrics_page_ = false;
        player.lyrics_visible(false);
        equalizer_page_ = false;
        feedback.play(audio::Cue::modal_open);
        return;
    }

    if (input.is_pressed(Action::touch))
    {
        full_ = !full_;
        lyrics_page_ = false;
        player.lyrics_visible(false);
        album_panel_ = false;
        settings_ = false;
        speaker_page_ = false;
        equalizer_page_ = false;
        feedback.play(audio::Cue::select);
        return;
    }

    if (equalizer_page_)
    {
        if (input.is_pressed(Action::back))
        {
            equalizer_page_ = false;
            settings_ = true;
            feedback.play(audio::Cue::back);
            return;
        }

        if (input.nav == Direction::left)
            eq_focus_ = std::max(0, eq_focus_ - 1);

        if (input.nav == Direction::right)
            eq_focus_ = std::min(11, eq_focus_ + 1);

        if (eq_focus_ < 10 && (input.nav == Direction::up || input.nav == Direction::down))
            dsp.band(size_t(eq_focus_),
                     dsp.band(size_t(eq_focus_)) + (input.nav == Direction::up ? 1.f : -1.f));

        if (input.is_pressed(Action::confirm))
        {
            if (eq_focus_ < 10)
                dsp.band(size_t(eq_focus_), 0);

            if (eq_focus_ == 10)
                dsp.equalizer(!dsp.equalizer());

            if (eq_focus_ == 11)
                for (size_t i = 0; i < 10; ++i)
                    dsp.band(i, 0);
        }

        if (input.nav != Direction::none || input.is_pressed(Action::confirm))
            feedback.play(audio::Cue::slider);

        return;
    }

    if (!settings_ && input.is_pressed(Action::page_prev))
        player.next(-1);

    if (!settings_ && input.is_pressed(Action::page_next))
        player.next(1);

    if (input.is_pressed(Action::jump_prev))
        player.seek(-10);

    if (input.is_pressed(Action::jump_next))
        player.seek(10);

    if (album_panel_)
    {
        if (input.is_pressed(Action::back))
        {
            album_panel_ = false;
            feedback.play(audio::Cue::back);
        }
        if (input.nav == Direction::up || input.nav == Direction::down)
            album_scroll_ = std::clamp(album_scroll_ + (input.nav == Direction::down ? 51.6f : -51.6f),
                                       0.f, album_scroll_max_);
        return;
    }
    if (settings_)
    {
        if (speaker_page_)
        {
            if (input.is_pressed(Action::back))
            {
                speaker_page_ = false;
                feedback.play(audio::Cue::back);
                return;
            }
            if (speakers_.handle(input, feedback) == ui::Event::changed)
                dsp.channel_gain(size_t(speakers_.changed_id()), db_gain(speakers_.slider_value(speakers_.changed_id())));
            return;
        }
        if (input.is_pressed(Action::back))
        {
            settings_ = false;
            feedback.play(audio::Cue::back);
            return;
        }

        ui::Form &form = preferences_;
        const auto event = form.handle(input, feedback);

        const int id = form.changed_id();

        if (event == ui::Event::activated && id == 0)
        {
            equalizer_page_ = true;
            settings_ = false;
            return;
        }
        if (event == ui::Event::activated && id == 12)
        {
            speaker_page_ = true;
            return;
        }

        if (event == ui::Event::changed)
        {
            if (id == 1)
                dsp.upmix(preferences_.toggle_value(1));

            if (id == 2)
            {
                gapless_ = preferences_.toggle_value(2);
                player.gapless(gapless_);
            }

            if (id == 3)
            {
                reduced_motion_ = form.toggle_value(3);
                player.reduced_motion(reduced_motion_);
            }

            if (id == 4)
            {
                visual_ = form.choice_index(4) + 1;
                player.visualization(unsigned(visual_));
            }

            if (id == 5)
                player.volume(preferences_.slider_value(5) / 100.f);

            if (id == 6)
                player.choose_edition(size_t(form.choice_index(6)));
            if (id == 7)
                player.visual_rotation(form.toggle_value(7));
            if (id == 8)
                player.autoplay(form.toggle_value(8));
            if (id == 10)
                player.show_eject_button(form.toggle_value(10));
            if (id == 11)
                player.oled_visualizer(form.toggle_value(11));
            if (id == 14)
            {
                player.accent_color(unsigned(form.choice_index(14)));
                apply_accent(unsigned(form.choice_index(14)));
            }
        }

        return;
    }

    if (full_)
    {
        if (input.is_pressed(Action::r3))
        {
            lyrics_page_ = !lyrics_page_;
            player.lyrics_visible(lyrics_page_);
            feedback.play(audio::Cue::select);
            return;
        }
        if (input.is_pressed(Action::back))
        {
            if (lyrics_page_) lyrics_page_ = false;
            else full_ = false;
            player.lyrics_visible(false);
            return;
        }

        if (input.is_pressed(Action::confirm))
            player.toggle_pause();

        if (lyrics_page_)
        {
            if (shown_lyrics_ && !shown_lyrics_->synced &&
                (input.nav == Direction::up || input.nav == Direction::down))
                plain_scroll_ = std::clamp(plain_scroll_ + (input.nav == Direction::down ? 90.f : -90.f),
                    0.f, lyric_blocks_.empty() ? 0.f : lyric_blocks_.back().center);
            return;
        }
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            visual_ = 1 + (visual_ - 1 + (input.nav == Direction::right ? 1 : int(visualizer_count)-1)) % int(visualizer_count);
            player.visualization(unsigned(visual_));
        }

        return;
    }

    if (cover_focus_)
    {
        if (input.nav == Direction::right || input.is_pressed(Action::back))
        {
            cover_focus_ = false;
            feedback.play(audio::Cue::focus);
        }
        if (input.is_pressed(Action::confirm) && album_available_)
        {
            album_panel_ = true;
            album_scroll_ = 0;
            album_scroll_motion_.snap(0);
            feedback.play(audio::Cue::modal_open);
        }
        return;
    }
    if (album_available_ && input.nav == Direction::left && (!toolbar_ || actions_.focus() == 0))
    {
        cover_focus_ = true;
        feedback.play(audio::Cue::focus);
        return;
    }
    if (input.is_pressed(Action::back))
    {
        toolbar_ = !toolbar_;
        feedback.play(audio::Cue::focus);
        return;
    }

    if (toolbar_)
    {
        if (input.nav == Direction::down)
        {
            toolbar_ = false;
            return;
        }

        if (actions_.handle(input, feedback) == ui::Event::activated)
        {
            switch (actions_.focus())
            {
            case 0:
                player.next(-1);
                break;

            case 1:
                player.toggle_pause();
                break;

            case 2:
                player.next(1);
                break;

            case 3:
                full_ = true;
                idle_ = 0;
                break;

            case 4:
                settings_ = true;
                break;

            case 5:
                player.eject_disc();
                break;
            }
        }
    }
    else
    {
        if (input.nav == Direction::up && tracks_.focus() == 0)
        {
            toolbar_ = true;
            return;
        }

        if (tracks_.handle(input, feedback) == ui::Event::activated)
            player.play_track(size_t(tracks_.focus()));
    }
}

void Screen::draw(const Snapshot &snapshot, const std::array<float, 32> &spectrum, Dsp &dsp,
                  const std::string &clock, int width, int height, GLuint framebuffer,
                  const std::array<float,256> &waveform)
{
    page_.clear();
    overlay_.clear();

    page_.rounded_rect({0, 0, 1920, 1080}, 0, gfx::Color::rgb(0));

    visualizer_.waveform(waveform);
    visualizer_.update(spectrum, last_dt_, snapshot.playing, reduced_motion_);

    if (equalizer_page_)
    {
        ui::Painter paint(page_, fonts_, theme_);

        ui::text(page_, fonts_.display, "Equalizer", 96, 114, 48, ink);

        paint.toggle({1570, 91, 94, 48}, dsp.equalizer() ? 1.f : 0.f,
                     {eq_focus_ == 10 ? 1.f : 0.f, 0, false});
        ui::text(page_, fonts_.regular, "Enabled", 1536, 124, 24, muted, gfx::Align::right);
        paint.button({1710, 78, 114, 68}, "", ui::ButtonKind::secondary,
                     {eq_focus_ == 11 ? 1.f : 0.f, 0, false});
        draw_icon(page_, {1739, 86, 56, 52}, DotIcon::reset, eq_focus_ == 11 ? accent : ink);
        for (int db = -12; db <= 12; db += 6)
        {
            const float y = 540 - float(db) * 18;
            page_.line(190, y, 1735, y, .65f, gfx::Color::rgb(0xffffff, db == 0 ? .20f : .085f));
            char label[20];
            std::snprintf(label, sizeof(label), "%+d dB", db);
            ui::text(page_, fonts_.regular, label, 154, y + 8, 23, muted, gfx::Align::right);
        }
        for (size_t i = 0; i < 10; ++i)
        {
            const float x = 254 + float(i) * 154, db = dsp.band(i), y = 540 - eq_positions_[i] * 18;
            for (int dot = 0; dot <= 54; ++dot)
            {
                const float rail = 324 + float(dot) * 8;
                const bool raised = rail >= std::min(y, 540.f) && rail <= std::max(y, 540.f);
                page_.circle(x, rail, 1.35f,
                             raised ? accent.with_alpha(.7f) : gfx::Color::rgb(0xffffff, .20f));
            }
            const auto knob = eq_focus_ == int(i) ? accent : ink;
            for (int col = -4; col <= 4; ++col)
                for (int row = -1; row <= 1; ++row)
                    page_.circle(x + float(col) * 5, y + float(row) * 5, 1.25f, knob);
            if (eq_focus_ == int(i))
            {
                page_.circle(x - 31, y, 1.4f, ink);
                page_.circle(x + 31, y, 1.4f, ink);
            }
            char value[20], hz[20];
            std::snprintf(value, sizeof(value), "%+.0f", double(db));
            std::snprintf(
                hz, sizeof(hz), eq_frequencies[i] >= 1000 ? "%g kHz" : "%g Hz",
                double(eq_frequencies[i] >= 1000 ? eq_frequencies[i] / 1000 : eq_frequencies[i]));
            ui::text(page_, fonts_.display, value, x, 286, 28, eq_focus_ == int(i) ? accent : muted,
                     gfx::Align::center);
            ui::text(page_, fonts_.regular, hz, x, 820, 25, ink, gfx::Align::center);
        }
        ui::Hint hints[] = {{ui::Button::dpad, "Band and level"},
                            {ui::Button::cross, "Reset / select"},
                            {ui::Button::circle, "Settings"}};

        ui::draw_hints(page_, fonts_, dot_style(), hints, 3, 96, false);
    }
    else if (full_ && lyrics_page_)
        draw_lyrics(snapshot, clock);
    else if (full_)
    {
        const auto presentation = VisualizerPresentation::sample(
            snapshot.oled_visualizer, idle_, time_, snapshot.playing);
        if (!presentation.blank)
        {
            page_.push_transform(1, 0, 0, presentation.x, presentation.y);
            visualizer_.draw(page_, visual_, !reduced_motion_, fonts_);
            page_.pop_transform();
        }
        const float hint_opacity = presentation.hints;
        if (hint_opacity > 0)
        {
            page_.push_opacity(hint_opacity);
            ui::text(page_,fonts_.display,doto_.fit(Visualizer::name(unsigned(visual_)),28,1300),96,118,28,muted);
            page_.pop_opacity();
        }

        std::string title = snapshot.disc.album, artist = snapshot.disc.artist;

        for (const auto &t : snapshot.disc.tracks)
            if (t.number == snapshot.track)
            {
                title = t.title;
                artist = t.artist;
            }

        if (presentation.labels > 0)
        {
            page_.push_opacity(presentation.labels);
            ui::text(page_, fonts_.display, doto_.fit(title, 48, 1400), 96, 910, 48, ink);
            ui::text(page_, fonts_.regular,
                     doto_.fit(artist + " · " + snapshot.disc.album, 28, 1400), 98, 961, 28, muted);
            ui::text(page_, fonts_.display, clock, 1824,
                     centered_baseline(doto_, clock, 64.5f, 98), 64.5f, ink, gfx::Align::right);
            page_.pop_opacity();
        }

        ui::Hint hints[] = {{ui::Button::circle, "Back"},
                            {ui::Button::cross, "Play / pause"},
                            {ui::Button::dpad, "Effect"},
                            {ui::Button::r3, "Lyrics"}};


        if (hint_opacity > 0)
        {
            page_.push_opacity(hint_opacity);
            ui::draw_hints(page_, fonts_, dot_style(), hints, 4, 96, false);
            page_.pop_opacity();
        }
    }
    else
    {
        ui::text(page_, fonts_.display, "CD Player", 96,
                 centered_baseline(doto_, "CD Player", 64.5f, 111), 64.5f, ink);

        ui::text(page_, fonts_.display, clock, 1824, centered_baseline(doto_, clock, 64.5f, 111),
                 64.5f, ink, gfx::Align::right);

        if (media_motion_.disc() > .002f)
        {
            const float arrival = media_motion_.disc(), y = 230 + (1 - arrival) * 950;
            page_.push_clip({0, 205, 868, 875});
            const GLuint disc = scene_.render(spindle_.angle(), 1280, 1280);
            // The bare disc is centred in the artwork space; reveal moves it behind the cover.
            const float disc_x = 137 + 148 * media_motion_.cover();
            page_.image(disc, {disc_x, y - 25, 540, 540}, gfx::kCanvasUv, {1, 1, 1, arrival});
            if (cover_ && media_motion_.cover() > .001f)
            {
                const float reveal = media_motion_.cover(), scale = 1 + (1 - reveal) * .16f + .015f * cover_focus_amount_.value;
                page_.push_transform(scale, 336, 489, 0, 0);
                page_.push_opacity(reveal);
                page_.shadow({112, 293, 448, 448}, 8, 46, {0, 0, 0, .6f});
                page_.image(cover_, {112, 265, 448, 448}, gfx::kFullUv, {1, 1, 1, 1}, 8);
                page_.bordered_rect({112, 265, 448, 448}, 8, {0, 0, 0, 0}, 1,
                                    gfx::Color::rgb(0xffffff, .14f));
                page_.gradient_rect_h({112, 265, 10, 448}, 2, {1, 1, 1, .16f}, {0, 0, 0, 0});
                if (cover_focus_amount_.value > .001f)
                    page_.bordered_rect({106, 259, 460, 460}, 12, {0, 0, 0, 0}, 2,
                                        accent.with_alpha(cover_focus_amount_.value));
                page_.pop_opacity();
                page_.pop_transform();
            }
            page_.pop_clip();
        }
        else
        {
            page_.ring(407, 452, 178, 1.5f, gfx::Color::rgb(0xffffff, .10f));

            page_.ring(407, 452, 62, 1.5f, gfx::Color::rgb(0xffffff, .07f));

            ui::text(page_, fonts_.semibold, "Insert a CD", 407, 465, 36, ink,
                     gfx::Align::center);
        }

        ui::text(page_, fonts_.display, doto_.fit(snapshot.disc.album, 48, 730), 98, 793, 48, ink);

        ui::text(page_, fonts_.display, doto_.fit(snapshot.disc.artist, 29, 730), 100, 841, 29,
                 muted);

        const std::string details =
            snapshot.disc.tracks.empty()
                ? snapshot.message
                :

                snapshot.disc.year + "  ·  " + std::to_string(snapshot.disc.tracks.size()) +
                    " tracks  ·  " + duration(double(snapshot.disc.leadout) / 75.);

        ui::text(page_, fonts_.display, doto_.fit(details, 23, 730), 100, 885, 23, ink);

        ui::Canvas canvas{page_, fonts_, 0, time_};

        actions_.draw(canvas);
        const auto current =
            std::find_if(snapshot.disc.tracks.begin(), snapshot.disc.tracks.end(),
                         [&](const auto &t) { return t.number == snapshot.track; });
        const double total =
            current == snapshot.disc.tracks.end() ? 0 : double(current->end - current->start) / 75.;
        // Match the visible track-number and duration columns, rather than the selection box.
        ui::text(page_, fonts_.display, duration(snapshot.seconds), 952, 374, 26, ink);
        ui::text(page_, fonts_.display, duration(total), 1776, 374, 26, muted, gfx::Align::right);
        const float progress =
            total > 0 ? float(std::clamp(snapshot.seconds / total, 0., 1.)) * 95 : 0;
        for (int dot = 0; dot < 96; ++dot)
        {
            const float lit = std::clamp(progress - dot + 1, 0.f, 1.f);
            const float cursor = std::max(0.f, 1 - std::abs(progress - dot));
            auto color = gfx::mix(gfx::Color::rgb(0xffffff, .20f), ink, lit);
            color = gfx::mix(color, accent, cursor);
            page_.circle(1080 + float(dot) * 6.1f, 366, 1.7f, color);
        }

        if (!snapshot.disc.tracks.empty())
            tracks_.draw(canvas);

        else
        {
            ui::text(page_, fonts_.regular, doto_.fit(snapshot.message, 30, 860), 940, 435, 30,
                     muted);

            ui::text(page_, fonts_.regular, "Tracks will appear here", 940, 487, 25, muted);
        }

        std::vector<ui::Hint> hints = {{ui::Button::cross, cover_focus_ ? "Album" : "Track"},
                                      {ui::Button::square, "Pause"}};
        if (snapshot.show_eject_button)
            hints.push_back({ui::Button::triangle, "Eject"});
        hints.push_back({ui::Button::l1, "Track", ui::Button::r1});
        hints.push_back({ui::Button::l2, "10 sec", ui::Button::r2});
        hints.push_back({ui::Button::options, "Settings"});

        ui::HintLayout layout;
        layout.size = 32;
        layout.text_size = 23;
        layout.cy = 1025;
        layout.item_gap = 32;

        ui::draw_hints(page_, fonts_, dot_style(), hints.data(), int(hints.size()), 96, false, layout);
    }

    renderer_.begin();
    renderer_.draw(page_);

    if (settings_amount_.value > .001f && !equalizer_page_)
    {
        const float amount = std::clamp(settings_amount_.value, 0.f, 1.f);
        renderer_.glass();
        overlay_.image(renderer_.glass_texture(), {0, 0, 1920, 1080}, gfx::kCanvasUv,
                       {1, 1, 1, amount});
        overlay_.rounded_rect({0, 0, 1920, 1080}, 0, {0, 0, 0, .36f * amount});
        overlay_.push_opacity(amount);
        overlay_.push_transform(1, 0, 0, (1 - amount) * 940, 0);
        overlay_.shadow({998, 78, 826, 930}, 26, 44, {0, 0, 0, .48f});
        overlay_.rounded_rect({998, 78, 826, 930}, 26, gfx::Color::rgb(0x1c1c1c));
        ui::text(overlay_, fonts_.display, speaker_page_ ? "Speaker setup" : "Settings", 1056, 148, 40, ink);
        ui::Canvas canvas{overlay_, fonts_, 0, time_};
        auto &form = speaker_page_ ? speakers_ : preferences_;
        form.draw(canvas);
        ui::text(overlay_, fonts_.regular, doto_.fit(form.help_text(), 21, 700), 1058,
                 948, 21, muted);
        ui::Hint hints[] = {{ui::Button::dpad, "Adjust"}, {ui::Button::circle, "Back"}};
        ui::HintLayout layout;
        layout.cy = 979;
        layout.size = 28;
        layout.text_size = 21;
        ui::draw_hints(overlay_, fonts_, dot_style(), hints, 2, 1056, false, layout);
        overlay_.pop_transform();
        overlay_.pop_opacity();
        renderer_.draw(overlay_);
    }
    if (album_amount_.value > .001f && !equalizer_page_)
    {
        const float amount = std::clamp(album_amount_.value, 0.f, 1.f);
        overlay_.clear();
        renderer_.glass();
        overlay_.image(renderer_.glass_texture(), {0, 0, 1920, 1080}, gfx::kCanvasUv,
                       {1, 1, 1, amount});
        overlay_.rounded_rect({0, 0, 1920, 1080}, 0, {0, 0, 0, .36f * amount});
        overlay_.push_opacity(amount);
        overlay_.push_transform(1, 0, 0, -(1 - amount) * 1850, 0);
        // Album card with a clipped year behind its content.
        overlay_.shadow({96, 78, 1036.8f, 930}, 26, 44, {0, 0, 0, .48f});
        overlay_.rounded_rect({96, 78, 1036.8f, 930}, 26, gfx::Color::rgb(0x1c1c1c));
        auto centered_baseline = [&](std::string_view text, float size, float center)
        {
            std::vector<gfx::GlyphQuad> glyphs;
            doto_.layout(text, 0, 0, size, gfx::Align::left, glyphs);
            float top = 0, bottom = 0;
            if (!glyphs.empty())
            {
                top = glyphs.front().y0;
                bottom = glyphs.front().y1;
                for (const auto &glyph : glyphs)
                {
                    top = std::min(top, glyph.y0);
                    bottom = std::max(bottom, glyph.y1);
                }
            }
            return center - (top + bottom) * .5f;
        };
        const auto description = ui::paragraph_lines(fonts_.display,
            snapshot.album_info.annotation, 33.6f, 940.8f, 600);
        const float description_bottom = 424.4f + float(description.lines.size() - 1) * 51.6f + 12;
        album_scroll_max_ = std::max(0.f, description_bottom + 20 - 934);
        album_scroll_ = std::clamp(album_scroll_, 0.f, album_scroll_max_);
        // All album content shares one viewport and one scroll transform.
        overlay_.push_clip({122, 104, 984.8f, 830});
        const float scroll_offset = std::clamp(album_scroll_motion_.value, 0.f, album_scroll_max_);
        overlay_.push_transform(1, 0, 0, 0, -scroll_offset);
        // Paint the year first, behind cover, artist, album and description.
        ui::text(overlay_, fonts_.display, snapshot.disc.year, 1084.8f,
                 centered_baseline(snapshot.disc.year, 240.24f, 238.64f), 240.24f,
                 accent.with_alpha(.20f), gfx::Align::right);
        if (cover_)
            overlay_.image(cover_, {144, 126, 225.28f, 225.28f}, gfx::kFullUv, {1, 1, 1, 1}, 8);
        // Wrap complete names, then shrink both sizes together until the
        // stacked artist/album heading fits inside the cover's 225.28px height.
        struct HeadingBlock
        {
            std::vector<std::string> lines;
            float size = 0, leading = 0, top = 0, bottom = 0;
        };
        auto heading_block = [&](std::string_view value, float size)
        {
            HeadingBlock block;
            block.size = size;
            block.leading = size * 1.25f;
            block.lines = doto_.wrap(value, size, 667.52f);
            bool measured = false;
            for (size_t i = 0; i < block.lines.size(); ++i)
            {
                std::vector<gfx::GlyphQuad> glyphs;
                doto_.layout(block.lines[i], 0, float(i) * block.leading,
                             size, gfx::Align::left, glyphs);
                for (const auto &glyph : glyphs)
                {
                    block.top = measured ? std::min(block.top, glyph.y0) : glyph.y0;
                    block.bottom = measured ? std::max(block.bottom, glyph.y1) : glyph.y1;
                    measured = true;
                }
            }
            return block;
        };
        float heading_scale = 1;
        HeadingBlock artist, album;
        float heading_height = 0, gap = 0;
        for (unsigned attempt = 0; attempt < 100; ++attempt)
        {
            artist = heading_block(snapshot.disc.artist, 37.8f * heading_scale);
            album = heading_block(snapshot.disc.album, 52.92f * heading_scale);
            gap = snapshot.disc.artist.empty() ? 0 : 12.6f * heading_scale;
            heading_height = artist.bottom - artist.top + gap + album.bottom - album.top;
            if (heading_height <= 225.28f) break;
            heading_scale *= .94f;
        }
        const float heading_top = 126 + (225.28f - heading_height) * .5f;
        ui::ParagraphLines artist_lines{artist.lines, false}, album_lines{album.lines, false};
        ui::paragraph(overlay_, fonts_.display, artist_lines, 417.28f,
                      heading_top - artist.top, artist.size, artist.leading, muted);
        ui::paragraph(overlay_, fonts_.display, album_lines, 417.28f,
                      heading_top + artist.bottom - artist.top + gap - album.top,
                      album.size, album.leading, ink);
        ui::paragraph(overlay_, fonts_.display, description, 144, 424.4f, 33.6f, 51.6f, ink);
        overlay_.pop_transform();
        overlay_.pop_clip();
        if (album_scroll_max_ > 0)
        {
            const float height = 830 * 830 / (album_scroll_max_ + 830);
            overlay_.rounded_rect({1108.8f, 104 + (830-height)*scroll_offset/album_scroll_max_, 2, height},
                                  1, muted.with_alpha(.5f));
        }
        // Back stays fixed outside the content scroll.
        ui::Hint hints[] = {{ui::Button::circle, "Back"}};
        ui::HintLayout layout;
        layout.size = 28; layout.text_size = 20;
        layout.cy = 963 - layout.text_size * .35f;
        ui::draw_hints(overlay_, fonts_, dot_style(), hints, 1, 144, false, layout);
        overlay_.pop_transform();
        overlay_.pop_opacity();
        renderer_.draw(overlay_);
    }
    if (volume_feedback_ > 0)
    {
        overlay_.clear();
        const auto label = "Volume " + std::to_string(int(std::round(snapshot.volume*100))) + "%";
        const float opacity = std::min(1.f, volume_feedback_/.3f);
        const float text_width = fonts_.display.measure(label, 23);
        overlay_.rounded_rect({1808-text_width, 997, text_width+32, 48}, 10, {0,0,0,.8f*opacity});
        ui::text(overlay_, fonts_.display, label, 1824, 1025, 23,
                 ink.with_alpha(opacity), gfx::Align::right);
        renderer_.draw(overlay_);
    }
    (void)dsp;
    renderer_.present(framebuffer, width, height);
}

} // namespace cd
