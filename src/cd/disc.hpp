// CD Player - Disc layout, MMC commands and album metadata.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace cd
{
constexpr unsigned sector_bytes = 2352;
constexpr unsigned sector_frames = 588;
struct Track
{
    unsigned number = 0;
    unsigned control = 0;
    uint32_t start = 0, end = 0;
    std::string title, artist;
    bool audio() const
    {
        return (control & 4) == 0;
    }
};
struct Disc
{
    std::vector<Track> tracks;
    unsigned first = 0, last = 0;
    uint32_t leadout = 0;
    std::string id, album = "CD Audio", artist, year, release;
    unsigned medium_position=1, medium_count=1;
};
struct Release
{
    std::string id, album, artist, date, country, description;
    std::vector<Track> tracks;
    unsigned medium_position=1, medium_count=1;
};
struct AlbumInfo
{
    std::string annotation, group_id, first_date, date, type, country, labels;
    std::string wikidata_id, source, source_url;
};
struct Command
{
    std::array<uint8_t, 16> bytes{};
    uint8_t length = 6;
    uint32_t allocation = 0;
    bool short_reply = false;
};
struct Reply
{
    bool ok = false;
    int system_error = 0;
    uint32_t transferred = 0;
    unsigned sense_key = 0, asc = 0, ascq = 0;
    std::string error;
    bool no_medium() const
    {
        return sense_key == 2 && asc == 0x3a;
    }
    bool changed() const
    {
        return sense_key == 6;
    }
};
class Drive
{
  public:
    virtual ~Drive() = default;
    virtual Reply execute(const Command &, std::span<uint8_t>) = 0;
    virtual std::string label() const = 0;
};
Command inquiry();
Command ready();
Command read_toc(bool text = false);
Command read_audio(uint32_t lba, unsigned sectors);
Command apple_wake();
Command unlock();
Command eject();
bool parse_toc(std::span<const uint8_t> data, Disc &, std::string &error);
bool parse_cdtext(std::span<const uint8_t>, Disc &);
std::string disc_id(const Disc &);
std::array<uint8_t, 20> sha1(std::span<const uint8_t>);
std::vector<Release> parse_releases(std::string_view json, const Disc &);
bool apply_release(const Release &, Disc &);
AlbumInfo parse_album_info(std::string_view release_json, std::string_view group_json,
                           std::string_view release_id);
std::string parse_wikipedia_title(std::string_view entity_json, std::string_view entity_id);
std::string parse_wikipedia_description(std::string_view summary_json);
std::string url_encode(std::string_view value);
std::unique_ptr<Drive> open_drive(const std::string &device, std::string &error);
std::vector<std::string> drive_names();
int http_get(const std::string &url, std::vector<uint8_t> &out, size_t limit);
} // namespace cd
