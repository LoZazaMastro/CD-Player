// CD Player - Exact-release artwork, partial availability and safe scan decoding.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/artwork.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iterator>

static void require(bool value,const char *message)
{
    if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::abort();}
}
int main()
{
    const std::string release="12345678-1234-1234-1234-123456789abc";
    const auto sources=cd::parse_artwork_sources(R"({"images":[
        {"id":1,"types":["Front"],"approved":true},
        {"id":2,"types":["Medium"],"approved":true,"comment":"CD 1"},
        {"id":"3","types":["Back","Spine"],"approved":true},
        {"id":4,"types":["Medium"],"approved":true,"comment":"CD 10"},
        {"id":5,"types":["Matrix/Runout","Medium"],"approved":true}
    ]})",release,1,2);
    require(sources.disc.ends_with("/2-1200.jpg"),"CD 1 cannot select CD 10 or runout");
    require(sources.back==sources.spine && sources.combined_spine && sources.back_has_spines,
            "combined tray scan identifies both back and spine");
    const auto missing=cd::parse_artwork_sources(R"({"images":[{"id":9,"types":["Front"]}]})",release);
    require(missing.disc.empty() && missing.back.empty() && missing.spine.empty(),"missing surfaces stay blank without front substitutions");
    require(cd::parse_artwork_sources(R"({"release":"https://musicbrainz.org/release/other","images":[]})",release).disc.empty(),"mismatched release rejected");
    require(cd::parse_artwork_sources("{",release).disc.empty(),"invalid manifest tolerated");
    require(cd::parse_artwork_sources(std::string((2u<<20)+1,'x'),release).disc.empty(),"manifest bound enforced");
    require(cd::parse_artwork_sources(R"({"images":[{"id":"../9","types":["Medium"]}]})",release).disc.empty(),"image identifiers cannot inject URLs");
    std::ifstream fixture("tests/fixtures/artwork-tray.png",std::ios::binary);
    std::vector<uint8_t> scan((std::istreambuf_iterator<char>(fixture)),{});
    const auto back=cd::decode_artwork(scan,cd::ArtworkCrop::back_with_spines);
    const auto spine=cd::decode_artwork(scan,cd::ArtworkCrop::spine_from_back);
    const auto disc=cd::decode_artwork(scan,cd::ArtworkCrop::disc);
    require(back && back->width==72 && back->height==64 && back->rgba[1]==255 && back->rgba[0]==0,"back preserves wider inlay and removes narrow folded spines");
    require(spine && spine->width==4 && spine->height==64 && spine->rgba[0]==255 && spine->rgba[1]==0,"spine is extracted independently from scan edge");
    require(disc && disc->width==64 && disc->height==64 && disc->rgba[1]==255,"disc scan gets centered square crop");
    std::vector<uint8_t> corrupt(32,255);
    require(!cd::decode_artwork(corrupt,cd::ArtworkCrop::disc),"corrupt image leaves surface blank");
    require(!cd::decode_artwork(std::vector<uint8_t>((8u<<20)+1),cd::ArtworkCrop::disc),"image byte bound enforced");
    std::puts("Artwork passed: exact release and medium, missing surfaces, manifest limits, independent back/spine crops, corrupt images");
}
