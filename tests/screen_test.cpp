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
    static bool lyrics(const Screen &s) { return s.lyrics_page_; }
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
        hui::InputFrame menu;menu.pressed=hui::action_bit(hui::Action::menu);
        screen.handle(menu,feedback,player,dsp);require(!cd::ScreenInspection::lyrics(screen),"settings leave lyrics mode");
        settle();dsp.upmix(true);settle();
        cd::ScreenInspection::preference(screen,12);
        hui::InputFrame increase;increase.nav=hui::Direction::right;
        screen.handle(increase,feedback,player,dsp);
        require(dsp.surround_gain()>1 && dsp.lfe_gain()==1,"surround slider changes only additional channels gain");
        const float surround_gain=dsp.surround_gain();
        cd::ScreenInspection::preference(screen,13);screen.handle(increase,feedback,player,dsp);
        require(dsp.lfe_gain()>1 && dsp.surround_gain()==surround_gain,"subwoofer slider operates independently");
    }
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context);eglTerminate(display);
    std::puts("Screen passed: transition-only reveal, free browsing, pause/resume, OLED hide/drift/black/wake");
}
