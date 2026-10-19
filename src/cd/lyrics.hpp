// CD Player - Bounded lyrics decoding and playback-position lookup.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cd/disc.hpp"
#include <string_view>

namespace cd
{
struct LyricWord
{
    double seconds = 0;
    std::string text;
};
struct LyricLine
{
    double seconds = 0;
    std::string text;
    std::vector<LyricWord> words;
};
struct Lyrics
{
    std::vector<LyricLine> lines;
    bool synced = false, instrumental = false;
};
enum class LyricsState { idle, loading, ready, unavailable };
Lyrics parse_lrc(std::string_view text);
Lyrics parse_lyrics_response(std::string_view json, const Track &, const Disc &);
int lyric_line_at(const Lyrics &, double seconds);
std::string lyrics_url(const Track &, const Disc &);
std::string lyrics_cache_key(const Track &, const Disc &);
} // namespace cd
