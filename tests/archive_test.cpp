// CD Player - Exercise every original archive program through the native adapter.
// Copyright (C) 2026 Andrea.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/archive_studies.hpp"
#include "app/archive_catalog.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include <cstdio>
#include <cmath>
#include <fstream>
int main(){
    cd::ArchiveStudies studies;studies.assets("assets");
    hui::gfx::Font font;std::string bytes;if(!hui::save::read_file("assets/fonts/doto-semibold.huifont",&bytes)||!font.load(bytes))return 2;
    hui::ui::Fonts fonts;fonts.display={&font,0xf0000001};fonts.regular=fonts.display;fonts.semibold=fonts.display;
    unsigned failed=0;double maximum=0;
    std::ofstream report("build/archive-study-validation.tsv");report<<"index\tname\tresult\tinstances\tvertices\tmax_frame_ms\n";
    for(unsigned selected=1;selected<=cd::archive_entries.size();selected++){
        bool seen=false,okay=true;double worst=0;size_t instances=0,vertices=0;
        for(unsigned frame=0;frame<35;frame++){
            hui::gfx::DrawList out;std::array<float,32> bands;for(unsigned i=0;i<32;i++)bands[i]=.1f+.7f*std::abs(std::sin(float(i)*.7f+frame*.1f));
            auto start=hui::sys::monotonic_us();
            bool drawn=studies.draw(out,selected,frame/30.f,.65f,.45f,.25f,.55f,frame/15,float(frame%15)/30,float(frame%11)/30,bands,fonts);
            double ms=(hui::sys::monotonic_us()-start)/1000.;worst=std::max(worst,ms);
            if(!studies.error().empty()){okay=false;break;}seen|=drawn;
            instances=std::max(instances,out.instances().size());vertices=std::max(vertices,out.mesh_vertices().size());
            for(const auto &v:out.mesh_vertices())if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.a))okay=false;
            for(const auto &v:out.instances())for(float x:v.rect)if(!std::isfinite(x))okay=false;
        }
        okay&=seen;maximum=std::max(maximum,worst);if(!okay){++failed;std::fprintf(stderr,"FAILED %u %.*s: %s\n",selected,int(cd::archive_entries[selected-1].title.size()),cd::archive_entries[selected-1].title.data(),studies.error().c_str());}
        report<<selected<<'\t'<<cd::archive_entries[selected-1].title<<'\t'<<(okay?"PASS":"FAIL")<<'\t'<<instances<<'\t'<<vertices<<'\t'<<worst<<'\n';
    }
    std::printf("Archive programs: %zu, failed: %u, longest host frame including first load: %.2f ms\n",cd::archive_entries.size(),failed,maximum);
    return failed?1:0;
}
