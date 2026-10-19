// CD Player - Offscreen preview using the same interface and shaders as PS5.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/screen.hpp"
#include "gfx/gl_program.hpp"
#include "core/save_file.hpp"
#define JSON_NOEXCEPTION 1
#include "third_party/nlohmann/json.hpp"
#include "third_party/stb/stb_image.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"
#pragma clang diagnostic pop
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace cd
{
std::vector<std::string> drive_names()
{
    return {};
}
std::unique_ptr<Drive> open_drive(const std::string &, std::string &error)
{
    error = "Host preview";
    return {};
}
int http_get(const std::string &, std::vector<uint8_t> &, size_t)
{
    return -1;
}
} // namespace cd
int main()
{
    std::fprintf(stderr, "CD Player preview: opening EGL\n");
    const auto get = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDisplay display = get(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    EGLint major = 0, minor = 0;
    if (!eglInitialize(display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
    {
        std::fprintf(stderr, "EGL init failed %#x\n", eglGetError());
        return 2;
    }
    const EGLint attrs[] = {EGL_CONTEXT_MAJOR_VERSION,
                            4,
                            EGL_CONTEXT_MINOR_VERSION,
                            5,
                            EGL_CONTEXT_OPENGL_PROFILE_MASK,
                            EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                            EGL_NONE};
    EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attrs);
    if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
    {
        std::fprintf(stderr, "EGL context failed %#x\n", eglGetError());
        return 3;
    }
    hui::gfx::set_glsl_prefix("#version 450 core\n");
    cd::Screen screen;
    if (!screen.init("assets"))
        return 4;
    cd::Scene disc_reference;
    if(!disc_reference.init("assets")) return 7;
    for(unsigned phase=0;phase<4;++phase)
    {
        GLuint texture=disc_reference.render(float(phase)*.85f,512,512);
        glBindTexture(GL_TEXTURE_2D,texture);
        std::vector<uint8_t> pixels(512*512*4);
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        stbi_flip_vertically_on_write(1);
        std::filesystem::create_directories("build/previews");
        const std::string file="build/previews/disc-phase-"+std::to_string(phase)+".png";
        stbi_write_png(file.c_str(),512,512,4,pixels.data(),512*4);
        if(glGetError()!=GL_NO_ERROR) return 8;
    }
    cd::Dsp dsp;
    cd::Snapshot snapshot;
    snapshot.state = cd::State::ready;
    snapshot.revision = 1;
    std::ifstream source("build/apple-disc.json");
    nlohmann::json probe;
    source >> probe;
    auto &disc = snapshot.disc;
    disc.id = probe["disc_id"].get<std::string>();
    disc.first = 1;
    disc.last = 10;
    for (const auto &row : probe["toc"])
    {
        if (row["number"] == 170)
        {
            disc.leadout = row["lba"];
            break;
        }
        cd::Track t;
        t.number = row["number"];
        t.start = row["lba"];
        disc.tracks.push_back(t);
    }
    for (size_t i = 0; i < disc.tracks.size(); ++i)
        disc.tracks[i].end = i + 1 < disc.tracks.size() ? disc.tracks[i + 1].start : disc.leadout;
    std::string metadata;
    hui::save::read_file("build/apple-disc.musicbrainz.json", &metadata);
    snapshot.editions = cd::parse_releases(metadata, disc);
    if (snapshot.editions.empty())
        return 5;
    cd::apply_release(snapshot.editions[0], disc);
    const cd::Disc reference_disc = disc;
    int w = 0, h = 0, channels = 0;
    auto *pixels = stbi_load("build/test-cover.jpg", &w, &h, &channels, 4);
    if (pixels)
    {
        snapshot.cover = std::make_shared<std::vector<uint8_t>>(pixels, pixels + size_t(w) * h * 4);
        snapshot.cover_width = w;
        snapshot.cover_height = h;
        stbi_image_free(pixels);
    }
    snapshot.cover_revision = 1;
    snapshot.drive = "Apple USB SuperDrive";
    snapshot.track = 1;
    snapshot.seconds = 85;
    snapshot.playing = true;
    cd::Analyzer analyzer;
    std::ifstream wav("build/apple-disc.wav", std::ios::binary);
    std::vector<char> audio((std::istreambuf_iterator<char>(wav)), {});
    for (size_t i = 44; i + 3 < audio.size(); i += 4)
    {
        const auto *p = reinterpret_cast<const uint8_t *>(audio.data() + i);
        analyzer.feed(float(int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8))) / 32768.f,
                      float(int16_t(uint16_t(p[2]) | (uint16_t(p[3]) << 8))) / 32768.f);
    }
    if(audio.empty())
        for(unsigned i=0;i<48000;++i)
        {
            const float t=float(i)/48000;
            const float v=.35f*std::sin(t*2*3.14159265f*110)+.2f*std::sin(t*2*3.14159265f*440);
            analyzer.feed(v,v);
        }
    for (int i = 0; i < 180; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    hui::gfx::Canvas target;
    if (!target.create(1920, 1080, 1))
        return 6;
    std::filesystem::create_directories("build/previews");
    auto capture = [&](const char *name)
    {
        screen.draw(snapshot, analyzer.bands(), dsp, "21:30", 1920, 1080, target.framebuffer(),analyzer.waveform());
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer());
        std::vector<uint8_t> rgba(1920 * 1080 * 4);
        glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        stbi_flip_vertically_on_write(1);
        const std::string path = std::string("build/previews/") + name + ".png";
        stbi_write_png(path.c_str(), 1920, 1080, 4, rgba.data(), 1920 * 4);
        std::printf("%s GL=0x%x\n", path.c_str(), glGetError());
    };
    capture("album");
    snapshot.show_eject_button=false;
    for(int frame=0;frame<60;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("album-no-eject");
    snapshot.show_eject_button=true;
    for(int frame=0;frame<60;++frame)screen.update(1.f/60,snapshot,dsp);
    // Preview the actual currently inserted album with the production parsers.
    auto original = snapshot;
    std::string release_bytes, group_bytes, entity_bytes, summary_bytes, disc_bytes;
    hui::save::read_file("build/http-research/album-info-release.json", &release_bytes);
    hui::save::read_file("build/http-research/album-info-group.json", &group_bytes);
    hui::save::read_file("build/http-research/album-wikidata.json", &entity_bytes);
    hui::save::read_file("build/http-research/album-wikipedia.json", &summary_bytes);
    hui::save::read_file("build/http-research/current-disc.musicbrainz.json", &disc_bytes);
    const auto album_disc = nlohmann::json::parse(disc_bytes,nullptr,false);
    snapshot.disc = {};
    snapshot.disc.id = "fns.RY9HGhCefEk8AmNQKlv7DHE-";
    snapshot.disc.first=1;snapshot.disc.last=13;snapshot.disc.leadout=188829;
    for(unsigned i=0;i<13;++i)
    {
        cd::Track track;track.number=i+1;track.start=album_disc["offsets"][i].get<uint32_t>()-150;
        track.end=i<12?album_disc["offsets"][i+1].get<uint32_t>()-150:188829;
        snapshot.disc.tracks.push_back(track);
    }
    snapshot.editions=cd::parse_releases(disc_bytes,snapshot.disc);
    cd::apply_release(snapshot.editions.front(),snapshot.disc);
    snapshot.album_info=cd::parse_album_info(release_bytes,group_bytes,snapshot.disc.release);
    snapshot.album_info.annotation=cd::parse_wikipedia_description(summary_bytes);
    snapshot.album_info.source="Wikipedia";
    snapshot.album_info_ready=true;
    auto *album_pixels=stbi_load("build/preview-blunderbuss.jpg",&w,&h,&channels,4);
    if(!album_pixels)return 9;
    snapshot.cover=std::make_shared<std::vector<uint8_t>>(album_pixels,album_pixels+size_t(w)*h*4);
    snapshot.cover_width=w;snapshot.cover_height=h;stbi_image_free(album_pixels);
    ++snapshot.revision;++snapshot.cover_revision;
    for(int i=0;i<180;++i)screen.update(1.f/60,snapshot,dsp);
    if (std::getenv("CD_PLAYER_ARTWORK_PREVIEW"))
    {
        const auto load_art=[](const char *path,cd::ArtworkCrop crop)
        {
            std::ifstream file(path,std::ios::binary);
            const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
            return cd::decode_artwork(bytes,crop);
        };
        snapshot.playing=false;
        snapshot.disc_art=load_art("build/disc-art-research/blunderbuss-disc.jpg",cd::ArtworkCrop::disc);
        snapshot.back_art=load_art("build/disc-art-research/blunderbuss-back.jpg",cd::ArtworkCrop::back_with_spines);
        snapshot.spine_art=load_art("build/disc-art-research/blunderbuss-back.jpg",cd::ArtworkCrop::spine_from_back);
        screen.update(1.f/120,snapshot,dsp);capture("artwork-loading");
        for(int frame=0;frame<48;++frame)screen.update(1.f/120,snapshot,dsp);
        capture("artwork-fade");
        for(int frame=0;frame<240;++frame)screen.update(1.f/120,snapshot,dsp);
        capture("artwork-front");
        cd::Player art_player;hui::ui::Feedback art_feedback;
        hui::InputFrame flip;flip.pressed=hui::action_bit(hui::Action::l3);
        screen.handle(flip,art_feedback,art_player,dsp);
        for(int frame=0;frame<8;++frame)screen.update(1.f/60,snapshot,dsp);
        capture("artwork-turn");
        for(int frame=0;frame<12;++frame)screen.update(1.f/60,snapshot,dsp);
        capture("artwork-turn-back");
        for(int frame=0;frame<160;++frame)screen.update(1.f/60,snapshot,dsp);
        capture("artwork-back");
        screen.handle(flip,art_feedback,art_player,dsp);
        for(int frame=0;frame<180;++frame)screen.update(1.f/60,snapshot,dsp);
    }
    if (std::getenv("CD_PLAYER_RELEASE_GALLERY"))
    {
        snapshot.seconds=45;
        original=snapshot;
        capture("release-album");
        snapshot.show_eject_button=false;
        for(int frame=0;frame<60;++frame)screen.update(1.f/60,snapshot,dsp);
        capture("release-album-no-eject");
        snapshot.show_eject_button=true;
        for(int frame=0;frame<60;++frame)screen.update(1.f/60,snapshot,dsp);
    }
    hui::InputFrame select_cover;select_cover.nav=hui::Direction::left;
    cd::Player preview_player;hui::ui::Feedback preview_feedback;
    screen.handle(select_cover,preview_feedback,preview_player,dsp);
    for(int i=0;i<60;++i)screen.update(1.f/60,snapshot,dsp);
    capture("album-cover-focus");
    hui::InputFrame activate_cover;activate_cover.pressed=hui::action_bit(hui::Action::confirm);
    screen.handle(activate_cover,preview_feedback,preview_player,dsp);
    for(int i=0;i<60;++i)screen.update(1.f/60,snapshot,dsp);
    capture("album-info");
    const auto standard_annotation = snapshot.album_info.annotation;
    // Exercise overflowing notes independently of the short real album extract.
    snapshot.album_info.annotation += "\n\n" + standard_annotation;
    capture("album-info-scroll-before");
    hui::InputFrame scroll_album;scroll_album.nav=hui::Direction::down;
    for(int step=0;step<4;++step)screen.handle(scroll_album,preview_feedback,preview_player,dsp);
    screen.update(1.f/60,snapshot,dsp);
    capture("album-info-scroll-start");
    for(int frame=0;frame<5;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("album-info-scroll-mid");
    for(int frame=0;frame<120;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("album-info-scrolled");
    snapshot.album_info.annotation = standard_annotation;
    const auto standard_album = snapshot.disc.album, standard_artist = snapshot.disc.artist;
    snapshot.disc.artist = "The Orchestra of Exceptionally Long Artist Names and Their Very Special Guests";
    snapshot.disc.album = "An Exceptionally Long Album Title That Continues Across Several Lines With Additional Words for the Complete Deluxe Anniversary Edition";
    screen.preview_album_panel(false);
    screen.preview_album_panel(true);
    hui::InputFrame reset_scroll;reset_scroll.nav=hui::Direction::up;
    for(int step=0;step<10;++step)screen.handle(reset_scroll,preview_feedback,preview_player,dsp);
    for(int frame=0;frame<120;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("album-info-long-titles");
    snapshot.disc.album=standard_album;snapshot.disc.artist=standard_artist;
    screen.preview_album_panel(false);
    snapshot=original;++snapshot.revision;++snapshot.cover_revision;
    for(int i=0;i<180;++i)screen.update(1.f/60,snapshot,dsp);
    if(std::getenv("CD_PLAYER_RELEASE_GALLERY")) snapshot.visualization=17;
    screen.preview_fullscreen(true);
    capture("visualizer-hints");
    for (int i = 0; i < 180; ++i)
    {
        screen.update(1.f / 60, snapshot, dsp);
        screen.draw(snapshot, analyzer.bands(), dsp, "21:30", 1920, 1080, target.framebuffer(),analyzer.waveform());
    }
    capture("visualizer");
    cd::Player simulated_player;
    hui::ui::Feedback feedback;
    hui::InputFrame change;
    change.nav = hui::Direction::right;
    screen.handle(change, feedback, simulated_player, dsp);
    capture("visualizer-signal-assembly");
    screen.handle(change, feedback, simulated_player, dsp);
    capture("visualizer-phase-mechanics");
    snapshot.visualization=14;
    for(int frame=0;frame<100;++frame){screen.update(1.f/60,snapshot,dsp);screen.draw(snapshot,analyzer.bands(),dsp,"21:30",1920,1080,target.framebuffer(),analyzer.waveform());}
    capture("visualizer-terminal");
    snapshot.oled_visualizer=true;
    for(int frame=0;frame<180;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("visualizer-oled");
    snapshot.playing=false;
    for(int frame=0;frame<3601;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("visualizer-oled-black");
    snapshot.playing=true;snapshot.oled_visualizer=false;snapshot.visualization=17;
    // Self-authored timing fixture: exercise the real lyrics layout without bundling song lyrics.
    snapshot.lyrics = std::make_shared<const cd::Lyrics>(cd::parse_lrc(
        "[00:00.00]A little room for music\n[00:05.00]Let the record carry you\n"
        "[00:10.00]Every song finds its own way home\n[00:15.00]And the evening opens slowly\n"
        "[00:20.00]One more moment, one more song"));
    snapshot.lyrics_state=cd::LyricsState::ready;snapshot.seconds=10.5;
    screen.preview_lyrics(true);
    for(int frame=0;frame<120;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("lyrics");
    screen.preview_lyrics(true);capture("lyrics-hints");
    snapshot.lyrics.reset();snapshot.lyrics_state=cd::LyricsState::unavailable;
    screen.update(1.f/60,snapshot,dsp);capture("lyrics-unavailable");
    screen.preview_fullscreen(false);
    screen.preview_preferences(true);
    for (int i = 0; i < 60; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    capture("settings");
    screen.preview_speakers(true);
    dsp.upmix(true);
    const float preview_db[] = {0,0,10,20,12,24,-10,-48};
    for(size_t c=0;c<8;++c)dsp.channel_gain(c,std::pow(10.f,preview_db[c]/20));
    for(int frame=0;frame<120;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("speaker-setup");
    screen.preview_speakers(false);
    screen.preview_preferences(true);
    hui::InputFrame color_nav;color_nav.nav=hui::Direction::down;
    for(int step=0;step<24;++step)screen.handle(color_nav,feedback,simulated_player,dsp);
    color_nav.nav=hui::Direction::up;screen.handle(color_nav,feedback,simulated_player,dsp);
    snapshot.accent_color=24;
    for(int frame=0;frame<120;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("settings-accent");
    for(int step=0;step<6;++step)screen.handle(color_nav,feedback,simulated_player,dsp);
    for(int frame=0;frame<120;++frame)screen.update(1.f/60,snapshot,dsp);
    capture("settings-volume");
    snapshot.accent_color=0;
    screen.preview_preferences(false);
    screen.preview_equalizer(true);
    dsp.equalizer(true);
    dsp.band(1, 3);
    dsp.band(4, -2);
    dsp.band(6, 2);
    for (int i = 0; i < 60; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    capture("equalizer");
    screen.preview_equalizer(false);
    snapshot.playing = false;
    snapshot.state = cd::State::empty;
    snapshot.disc = {};
    snapshot.cover.reset();
    snapshot.cover_revision++;
    snapshot.revision++;
    snapshot.message = "Insert an audio CD";
    for (int i = 0; i < 120; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    capture("empty");
    snapshot.state = cd::State::loading;
    snapshot.message = "Reading tracks";
    for (int i = 0; i < 12; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    capture("disc-insertion");
    for (int i = 0; i < 40; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    capture("disc-reading");
    snapshot.state = cd::State::ready;
    snapshot.disc = reference_disc;
    snapshot.cover_revision++;
    auto *cover = stbi_load("build/test-cover.jpg", &w, &h, &channels, 4);
    if (cover)
    {
        snapshot.cover = std::make_shared<std::vector<uint8_t>>(cover, cover + size_t(w) * h * 4);
        snapshot.cover_width = w;
        snapshot.cover_height = h;
        stbi_image_free(cover);
    }
    for (int i = 0; i < 8; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    capture("cover-reveal");
    for (int i = 0; i < 45; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    snapshot.state = cd::State::empty;
    snapshot.cover.reset();
    snapshot.disc = {};
    snapshot.cover_revision++;
    for (int i = 0; i < 18; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    capture("cover-removal");
    for (int i = 0; i < 24; ++i)
        screen.update(1.f / 60, snapshot, dsp);
    capture("disc-removal");
    return 0;
}
