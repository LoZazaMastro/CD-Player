// CD Player - Track following, OLED presentation and native drawing regression.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/screen.hpp"
#include "app/visualizer_presentation.hpp"
#include "gfx/gl_program.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cstdio>
#include <cstdlib>

namespace
{
void require(bool value, const char *message)
{
    if (!value) { std::fprintf(stderr,"FAIL: %s\n",message); std::abort(); }
}
}
namespace cd
{
std::vector<std::string> drive_names() { return {}; }
std::unique_ptr<Drive> open_drive(const std::string &, std::string &) { return {}; }
int http_get(const std::string &, std::vector<uint8_t> &, size_t) { return -1; }
struct ScreenInspection
{
    static int focus(const Screen &s) { return s.tracks_.focus(); }
    static bool visible(const Screen &s, int row)
    {
        const auto r=s.tracks_.row_rect(row), b=s.tracks_.bounds();
        return r.y >= b.y-.5f && r.y+r.h <= b.y+b.h+.5f;
    }
    static void idle(Screen &s,float value) { s.idle_=value; }
    static void preference(Screen &s,int id) { s.preferences_.focus_row(id); }
    static void speaker(Screen &s,int id) { s.speakers_.focus_row(id); }
    static bool speakers_open(const Screen &s) { return s.speaker_page_ && s.settings_; }
    static bool accent_matches(const Screen &s,unsigned index)
    {
        const auto color=hui::gfx::Color::rgb(accent_colors[index].rgb);
        return s.accent.r==color.r && s.accent.g==color.g && s.accent.b==color.b &&
            s.speakers_.style.theme.accent.r==color.r && s.preferences_.style.theme.accent.g==color.g &&
            s.tracks_.style.theme.accent.b==color.b && s.dot_style().label.r==color.r;
    }
    static bool volume_help(const Screen &s)
    {
        return s.preferences_.help_text()=="Right stick up/down adjusts volume" &&
            s.fonts_.regular.measure(s.preferences_.help_text(),21)<=700;
    }
    static bool interface_order(const Screen &s)
    {
        const int count=s.preferences_.row_count();
        return s.preferences_.row_at(count-2).id==14 && s.preferences_.row_at(count-1).id==10 &&
            s.preferences_.row(9)==nullptr;
    }
    static bool speaker_values_fit(const Screen &s)
    {
        for(const char *value : {"+10 dB", "+20 dB", "+24 dB", "-48 dB", "Mute"})
            if(s.fonts_.regular.measure(value,s.speakers_.style.value_size)>
                std::min(s.speakers_.style.number_width,s.speakers_.style.control_width*.4f)-8)return false;
        return true;
    }
    static bool lyrics(const Screen &s) { return s.lyrics_page_; }
    static bool fullscreen(const Screen &s) { return s.full_; }
    static bool panels(const Screen &s)
    {
        return s.settings_ && !s.overlay_.empty() && !s.volume_overlay_.empty() &&
            s.overlay_.instances().size()>s.volume_overlay_.instances().size();
    }
    static float flip(const Screen &s) { return s.cover_angle_.value; }
    static float disc_fade(const Screen &s) { return s.disc_surface_.fade; }
    static float back_fade(const Screen &s) { return s.back_surface_.fade; }
    static float spine_fade(const Screen &s) { return s.spine_surface_.fade; }
    static bool front_settled(const Screen &s) { return s.cover_ && !s.pending_cover_ && s.media_motion_.cover()>.99f; }
    static int line(const Screen &s) { return s.current_lyric_; }
    static float lyric_error(const Screen &s) { return s.lyric_scroll_.target-s.lyric_scroll_.value; }
    static size_t glyphs(const Screen &s)
    {
        size_t result=0;
        for(const auto &v:s.page_.instances())
            result+=v.params[3]==float(hui::gfx::Shape::glyph);
        return result;
    }
    static bool black(const Screen &s)
    {
        return s.page_.instances().size()==1 && s.page_.mesh_vertices().empty();
    }
};
}
int main()
{
    const auto get=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    require(get!=nullptr,"surfaceless EGL available");
    EGLDisplay display=get(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);
    EGLint major=0,minor=0;
    require(eglInitialize(display,&major,&minor) && eglBindAPI(EGL_OPENGL_API),"EGL initialized");
    const EGLint attrs[]={EGL_CONTEXT_MAJOR_VERSION,4,EGL_CONTEXT_MINOR_VERSION,5,
                         EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};
    EGLContext context=eglCreateContext(display,EGL_NO_CONFIG_KHR,EGL_NO_CONTEXT,attrs);
    require(eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,context),"GL context current");
    hui::gfx::set_glsl_prefix("#version 450 core\n");
    {
        cd::Screen screen; cd::Dsp dsp; cd::Player player;
        require(screen.init("assets"),"production screen initializes with new font");
        {
            hui::gfx::GlBatch batch;
            std::vector<uint8_t> red(64*64*4,255),green=red,blue=red;
            for(size_t i=0;i<red.size();i+=4){red[i+1]=red[i+2]=0;green[i]=green[i+2]=0;blue[i]=blue[i+1]=0;}
            const auto front=batch.create_texture(64,64,red.data()), back_art=batch.create_texture(64,64,green.data()), spine_art=batch.create_texture(64,64,blue.data());
            cd::CoverScene jewel;require(jewel.init(),"case shader initializes");
            const auto sample=[&](float angle,float fade){const auto texture=jewel.render(angle,front,back_art,spine_art,fade,fade,128);glBindTexture(GL_TEXTURE_2D,texture);std::vector<uint8_t> pixels(128*128*4);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return std::array<uint8_t,4>{pixels[(64*128+64)*4],pixels[(64*128+64)*4+1],pixels[(64*128+64)*4+2],pixels[(64*128+64)*4+3]};};
            const auto f=sample(0,1),b=sample(3.14159265f,1),s=sample(1.5707963f,1),blank=sample(3.14159265f,0),half=sample(3.14159265f,.5f);
            std::fprintf(stderr,"Case pixels front=%u/%u/%u/%u back=%u/%u/%u/%u spine=%u/%u/%u/%u GL=%x\n",f[0],f[1],f[2],f[3],b[0],b[1],b[2],b[3],s[0],s[1],s[2],s[3],glGetError());
            require(f[0]>180 && f[1]<60,"3D case front samples correct texture");
            require(b[1]>180 && b[0]<60,"3D case back samples correct texture");
            require(s[2]>180 && s[0]<60,"3D case spine samples correct texture");
            require(blank[1]<60,"back remains blank at start of fade");
            require(half[1]>blank[1]+60 && half[1]<b[1]-60,"back artwork visibly dissolves into the plastic surface");
            glDeleteTextures(1,&front);glDeleteTextures(1,&back_art);glDeleteTextures(1,&spine_art);
        }
        hui::gfx::Canvas target; require(target.create(1920,1080,1),"offscreen framebuffer ready");
        cd::Snapshot state;state.state=cd::State::ready;state.disc.id="screen-test";
        state.disc.album="Test album";state.track=1;state.playing=true;state.revision=1;
        for(unsigned i=0;i<16;++i){cd::Track t;t.number=i+1;t.title="Track "+std::to_string(i+1);t.start=i*4500;t.end=t.start+4500;state.disc.tracks.push_back(t);}
        const auto settle=[&](){for(int i=0;i<120;++i)screen.update(1.f/60,state,dsp);};
        settle();require(cd::ScreenInspection::visible(screen,0),"first track visible");
        state.track=9;settle();
        require(cd::ScreenInspection::visible(screen,8),"new audible track revealed without metadata revision");
        require(cd::ScreenInspection::focus(screen)==0,"automatic reveal preserves user selection");
        hui::InputFrame browse;browse.nav=hui::Direction::down;hui::ui::Feedback feedback;
        screen.handle(browse,feedback,player,dsp);settle();
        require(cd::ScreenInspection::focus(screen)==1 && cd::ScreenInspection::visible(screen,1),"user can browse away during playback");
        require(!cd::ScreenInspection::visible(screen,8),"steady playback never pulls the list back");
        state.playing=false;settle();state.playing=true;settle();
        require(cd::ScreenInspection::visible(screen,1),"pause and resume do not trigger follow");
        state.track=12;settle();require(cd::ScreenInspection::visible(screen,11),"next transition follows again");
        state.track=2;settle();require(cd::ScreenInspection::visible(screen,1),"backward transition reveals track");
        screen.preview_preferences(true);settle();
        cd::ScreenInspection::preference(screen,11);
        hui::InputFrame toggle;toggle.pressed=hui::action_bit(hui::Action::confirm);
        screen.handle(toggle,feedback,player,dsp);
        require(player.snapshot().oled_visualizer,"OLED toggle controls player preference");
        cd::ScreenInspection::preference(screen,10);screen.handle(toggle,feedback,player,dsp);
        require(!player.snapshot().show_eject_button,"Eject toggle controls player preference");
        screen.preview_preferences(false);settle();
        screen.preview_fullscreen(true);state.visualization=17;state.oled_visualizer=true;settle();
        std::array<float,32> spectrum{};spectrum.fill(.5f);
        const auto draw=[&](){screen.draw(state,spectrum,dsp,"21:30",1920,1080,target.framebuffer());require(glGetError()==GL_NO_ERROR,"drawing has no GL errors");};
        cd::ScreenInspection::idle(screen,0);draw();require(cd::ScreenInspection::glyphs(screen)>0,"OLED labels visible after input");
        cd::ScreenInspection::idle(screen,3);draw();require(cd::ScreenInspection::glyphs(screen)==0,"OLED hides clock, track, effect name and hints");
        require(!cd::ScreenInspection::black(screen),"playing OLED mode keeps effect visible");
        state.playing=false;cd::ScreenInspection::idle(screen,61);draw();
        require(cd::ScreenInspection::black(screen),"paused idle OLED mode draws only black");
        screen.handle(browse,feedback,player,dsp);draw();
        require(!cd::ScreenInspection::black(screen) && cd::ScreenInspection::glyphs(screen)>0,"any input wakes OLED presentation");
        state.oled_visualizer=false;cd::ScreenInspection::idle(screen,61);draw();
        require(cd::ScreenInspection::glyphs(screen)>0 && !cd::ScreenInspection::black(screen),"normal mode retains metadata and clock");
        const auto a=cd::VisualizerPresentation::sample(true,3,10,true), b=cd::VisualizerPresentation::sample(true,3,30,true);
        require(std::abs(a.x-b.x)>1 && std::abs(a.y-b.y)>1,"OLED effect moves in both axes");
        hui::InputFrame r3;r3.pressed=hui::action_bit(hui::Action::r3);
        screen.handle(r3,feedback,player,dsp);
        require(cd::ScreenInspection::lyrics(screen),"R3 opens lyrics from fullscreen visualizer");
        state.lyrics=std::make_shared<const cd::Lyrics>(cd::parse_lrc(
            "[00:01.00]First test phrase\n[00:04.00]Second test phrase\n[00:08.00]Third test phrase"));
        state.lyrics_state=cd::LyricsState::ready;state.seconds=2;state.playing=true;
        settle();draw();require(cd::ScreenInspection::line(screen)==0,"first sung phrase selected");
        state.seconds=5;screen.update(1.f/60,state,dsp);
        require(cd::ScreenInspection::line(screen)==1 && cd::ScreenInspection::lyric_error(screen)>1,
                "cue changes start smooth scrolling instead of snapping");
        settle();require(std::abs(cd::ScreenInspection::lyric_error(screen))<.1f,"current phrase settles at screen center");
        state.playing=false;settle();require(cd::ScreenInspection::line(screen)==1,"paused lyrics follow held audio position");
        state.seconds=1.5;settle();require(cd::ScreenInspection::line(screen)==0,"backward seek selects earlier phrase");
        screen.handle(r3,feedback,player,dsp);require(!cd::ScreenInspection::lyrics(screen),"R3 returns to visualizer");
        screen.handle(r3,feedback,player,dsp);
        hui::InputFrame leave;leave.pressed=hui::action_bit(hui::Action::back);
        screen.handle(leave,feedback,player,dsp);
        require(!cd::ScreenInspection::fullscreen(screen),"Circle from lyrics returns directly to album page");
        hui::InputFrame touch;touch.pressed=hui::action_bit(hui::Action::touch);
        screen.handle(touch,feedback,player,dsp);
        require(cd::ScreenInspection::fullscreen(screen) && cd::ScreenInspection::lyrics(screen),"fullscreen reopens last lyrics choice");
        screen.handle(r3,feedback,player,dsp);screen.handle(leave,feedback,player,dsp);
        screen.handle(touch,feedback,player,dsp);
        require(cd::ScreenInspection::fullscreen(screen) && !cd::ScreenInspection::lyrics(screen),"fullscreen also remembers visualizer choice");
        screen.handle(r3,feedback,player,dsp);
        hui::InputFrame menu;menu.pressed=hui::action_bit(hui::Action::menu);
        screen.handle(menu,feedback,player,dsp);require(!cd::ScreenInspection::lyrics(screen),"settings leave lyrics mode");
        settle();dsp.upmix(true);settle();
        cd::ScreenInspection::preference(screen,12);
        screen.handle(toggle,feedback,player,dsp);
        require(cd::ScreenInspection::speakers_open(screen),"Speaker setup opens inside settings drawer");
        settle();
        hui::InputFrame increase;increase.nav=hui::Direction::right;
        for(size_t channel=0;channel<8;++channel)
        {
            std::array<float,8> before;
            for(size_t c=0;c<8;++c)before[c]=dsp.channel_gain(c);
            cd::ScreenInspection::speaker(screen,int(channel));screen.handle(increase,feedback,player,dsp);
            for(size_t c=0;c<8;++c)
                require(c==channel?dsp.channel_gain(c)>before[c]:dsp.channel_gain(c)==before[c],"each speaker slider changes only its output channel");
            settle();
        }
        require(cd::ScreenInspection::speaker_values_fit(screen),"two-digit gain labels fit without ellipsis");
        draw();
        hui::InputFrame back;back.pressed=hui::action_bit(hui::Action::back);screen.handle(back,feedback,player,dsp);
        require(!cd::ScreenInspection::speakers_open(screen),"Back returns to main settings from speaker setup");
        cd::ScreenInspection::preference(screen,14);
        require(cd::ScreenInspection::interface_order(screen),"Accent color precedes Eject and designer row is removed");
        require(cd::accent_colors.size()==40,"forty named accent colors");
        for(unsigned step=1;step<=40;++step)
        {
            screen.handle(increase,feedback,player,dsp);
            const unsigned selected=step%40;
            require(player.snapshot().accent_color==selected && cd::ScreenInspection::accent_matches(screen,selected),
                    "accent selection immediately updates controls and hints");
            state.accent_color=selected;settle();draw();
        }
        cd::ScreenInspection::preference(screen,5);
        require(cd::ScreenInspection::volume_help(screen),"Volume shows right-stick instruction in settings help area");
        hui::InputFrame stick;stick.stick2_y=-1;player.volume(.5f);
        screen.handle(stick,feedback,player,dsp);require(player.volume()>.5f,"right stick raises volume within settings");
        draw();require(cd::ScreenInspection::panels(screen),"volume HUD retains separately queued settings geometry");
        glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer());
        std::array<uint8_t,4> panel_pixel{};
        glReadPixels(1200,1080-900,1,1,GL_RGBA,GL_UNSIGNED_BYTE,panel_pixel.data());
        require(panel_pixel[0]>15 && panel_pixel[1]>15,"settings card remains visible in final framebuffer while volume changes");
        screen.preview_preferences(false);screen.preview_fullscreen(false);settle();
        const float before_album=player.volume();stick.stick2_y=1;
        screen.handle(stick,feedback,player,dsp);require(player.volume()<before_album,"right stick lowers volume on album page");
        screen.preview_fullscreen(true);settle();const float before_visualizer=player.volume();stick.stick2_y=-1;
        screen.handle(stick,feedback,player,dsp);require(player.volume()>before_visualizer,"right stick raises volume in visualizer");
        screen.preview_lyrics(true);settle();const float before_lyrics=player.volume();stick.stick2_y=1;
        screen.handle(stick,feedback,player,dsp);require(player.volume()<before_lyrics,"right stick lowers volume in lyrics");
        player.volume(0);screen.handle(stick,feedback,player,dsp);require(player.volume()==0,"master volume cannot fall below mute");
        player.volume(1);stick.stick2_y=-1;screen.handle(stick,feedback,player,dsp);require(player.volume()==1,"master volume cannot exceed unity");
        stick.stick2_y=.1f;player.volume(.5f);screen.handle(stick,feedback,player,dsp);require(player.volume()==.5f,"stick deadzone does not change volume");
        state.volume=player.volume();draw();
        screen.preview_lyrics(false);screen.preview_preferences(false);settle();
        auto art=std::make_shared<cd::ArtworkImage>();art->width=art->height=64;
        art->rgba.resize(64*64*4,255);
        state.disc_art=state.back_art=state.spine_art=art;
        screen.update(1.f/120,state,dsp);
        require(cd::ScreenInspection::disc_fade(screen)==0 && cd::ScreenInspection::back_fade(screen)==0 &&
                cd::ScreenInspection::spine_fade(screen)==0,"all arriving artwork starts transparent");
        for(int frame=0;frame<36;++frame)screen.update(1.f/120,state,dsp);
        require(cd::ScreenInspection::disc_fade(screen)>.3f && cd::ScreenInspection::disc_fade(screen)<.7f &&
                cd::ScreenInspection::back_fade(screen)>.3f && cd::ScreenInspection::spine_fade(screen)<.7f,"disc, back and spine fade progressively at 120 Hz");
        hui::InputFrame l3;l3.pressed=hui::action_bit(hui::Action::l3);
        screen.handle(l3,feedback,player,dsp);screen.update(1.f/120,state,dsp);
        require(cd::ScreenInspection::flip(screen)>0 && cd::ScreenInspection::flip(screen)<1,"L3 starts gradual physical case rotation");
        settle();require(std::abs(cd::ScreenInspection::flip(screen)-3.14159265f)<.001f,"L3 settles on downloaded back");draw();
        screen.handle(l3,feedback,player,dsp);settle();require(cd::ScreenInspection::flip(screen)==0,"L3 flips back to front");
        state.cover_width=state.cover_height=64;
        state.cover=std::make_shared<const std::vector<uint8_t>>(64*64*4,220);++state.cover_revision;settle();
        state.cover=std::make_shared<const std::vector<uint8_t>>(64*64*4,180);++state.cover_revision;settle();settle();
        require(cd::ScreenInspection::front_settled(screen),"front replacement completes while back and spine remain available");
        state.back_art.reset();state.spine_art.reset();screen.update(1.f/120,state,dsp);draw();
        require(cd::ScreenInspection::back_fade(screen)==0 && cd::ScreenInspection::spine_fade(screen)==0,"missing surfaces stay blank independently");
    }
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context);eglTerminate(display);
    std::puts("Screen passed: transition-only reveal, free browsing, pause/resume, OLED hide/drift/black/wake");
}
