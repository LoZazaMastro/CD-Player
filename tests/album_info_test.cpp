// CD Player - Album identity and description availability regressions.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/disc.hpp"
#include <cassert>
#include <fstream>
#include <cstdio>
std::string read(const char *path)
{
    std::ifstream input(path);
    return {(std::istreambuf_iterator<char>(input)),{}};
}
int main()
{
    const auto release=read("tests/fixtures/album-info-release.json");
    const auto group=read("tests/fixtures/album-info-group.json");
    const auto info=cd::parse_album_info(release,group,"e9e2eaba-acfd-43ad-97c9-0817d94d4a36");
    assert(info.group_id=="37f48931-e5e6-488f-a531-ad2db311158d");
    assert(info.wikidata_id=="Q886366");
    assert(info.annotation.empty()); // No invented description from catalog fields.
    assert(info.labels.find("Third Man Records")!=std::string::npos);
    assert(cd::parse_album_info(release,group,"wrong-album").wikidata_id.empty());
    for(const auto *bad:{"", "[]", "{", "null", "123", "{\"id\":[]}"})
        assert(cd::parse_album_info(bad,group,"e9e2eaba-acfd-43ad-97c9-0817d94d4a36").annotation.empty());
    const auto entity=read("tests/fixtures/album-wikidata.json");
    const auto title=cd::parse_wikipedia_title(entity,"Q886366");
    assert(title=="Blunderbuss (album)");
    assert(cd::parse_wikipedia_title(entity,"Q123").empty());
    assert(cd::url_encode(title)=="Blunderbuss%20%28album%29");
    assert(cd::url_encode("a/b?c&d")=="a%2Fb%3Fc%26d");
    const auto summary=read("tests/fixtures/album-wikipedia.json");
    assert(cd::parse_wikipedia_description(summary).find("Jack White")!=std::string::npos);
    assert(cd::parse_wikipedia_description(R"({"type":"disambiguation","extract":"wrong album"})").empty());
    assert(cd::parse_wikipedia_description(R"({"type":"standard","extract":12})").empty());
    assert(cd::parse_wikipedia_description(R"({"type":"standard","extract":" \n "})").empty());
    const auto annotated=cd::parse_album_info(R"({"id":"r","annotation":"A ''note'' about [https://example.org|recording]."})",{},"r");
    assert(annotated.annotation=="A note about recording.");
    assert(annotated.source=="MusicBrainz");
    std::puts("Album information passed: correct release/group/entity identity, no-description gating, Wikipedia fallback, encoding and malformed data");
}
