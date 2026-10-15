// CD Player - Exercise every original archive program through the native adapter.
// Copyright (C) 2026 Andrea.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/archive_studies.hpp"
#include "app/archive_catalog.hpp"
#include "app/visualizer.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include <cstdio>
#include <cmath>
#include <fstream>
int main(){
    cd::ArchiveStudies studies;studies.assets("assets");
    hui::gfx::Font font;std::string bytes;if(!hui::save::read_file("assets/fonts/doto-semibold.huifont",&bytes)||!font.load(bytes))return 2;
    hui::ui::Fonts fonts;fonts.display={&font,0xf0000001};fonts.regular=fonts.display;fonts.semibold=fonts.display;
    hui::gfx::Font mono; if(!hui::save::read_file("assets/fonts/visualizer-mono.huifont",&bytes)||!mono.load(bytes))return 2;
    fonts.mono={&mono,0xf0000002};
    for(const auto cp:{0x2500u,0x2502u,0x250cu,0x2510u,0x2514u,0x2518u})if(!mono.has_glyph(cp))return 3;
    unsigned failed=0;double maximum=0;
    std::ofstream report("build/archive-study-validation.tsv");report<<"index\tname\tresult\tinstances\tvertices\tmax_frame_ms\tsteady_avg_ms\tsteady_max_ms\tredraws\tmissing_glyphs\n";
    for(unsigned selected=1;selected<=cd::archive_entries.size();selected++){
        bool seen=false,okay=true;double worst=0,steady=0,steady_max=0;size_t instances=0,vertices=0;unsigned missing=0;
        for(unsigned frame=0;frame<100;frame++){
            hui::gfx::DrawList out;std::array<float,32> bands;for(unsigned i=0;i<32;i++)bands[i]=.1f+.7f*std::abs(std::sin(float(i)*.7f+frame*.1f));
            auto start=hui::sys::monotonic_us();
            const float time=frame<50?frame/60.f:50/60.f+(frame-50)/120.f;
            bool drawn=studies.draw(out,selected,time,.65f,.45f,.25f,.55f,frame/15,float(frame%15)/60,float(frame%11)/60,bands,fonts);
            double ms=(hui::sys::monotonic_us()-start)/1000.;worst=std::max(worst,ms);
            if(frame>=5){steady+=ms;steady_max=std::max(steady_max,ms);}
            missing+=studies.missing_glyphs();
            if(!studies.error().empty()){okay=false;break;}seen|=drawn;
            instances=std::max(instances,out.instances().size());vertices=std::max(vertices,out.mesh_vertices().size());
            for(const auto &v:out.mesh_vertices())if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.a))okay=false;
            for(const auto &v:out.instances())for(float x:v.rect)if(!std::isfinite(x))okay=false;
        }
        okay&=seen && missing==0 && studies.redraws()>=94;maximum=std::max(maximum,worst);if(!okay){++failed;std::fprintf(stderr,"FAILED %u %.*s: redraws=%llu missing=%u %s\n",selected,int(cd::archive_entries[selected-1].title.size()),cd::archive_entries[selected-1].title.data(),(unsigned long long)studies.redraws(),missing,studies.error().c_str());}
        report<<selected<<'\t'<<cd::archive_entries[selected-1].title<<'\t'<<(okay?"PASS":"FAIL")<<'\t'<<instances<<'\t'<<vertices<<'\t'<<worst<<'\t'<<steady/95<<'\t'<<steady_max<<'\t'<<studies.redraws()<<'\t'<<missing<<'\n';
    }
    cd::Visualizer native;
    for(unsigned selected=153;selected<=cd::visualizer_count;++selected){
        hui::gfx::DrawList out;std::array<float,32> bands;bands.fill(.5f);
        native.update(bands,1.f/60,true,false);native.draw(out,int(selected),true,fonts);
        if(out.empty()){++failed;std::fprintf(stderr,"FAILED native study %u\n",selected);}
        for(const auto &v:out.instances())for(float x:v.rect)if(!std::isfinite(x))++failed;
    }
    std::printf("Archive programs: %zu, failed: %u, longest host frame including first load: %.2f ms\n",cd::archive_entries.size(),failed,maximum);
    return failed?1:0;
}
