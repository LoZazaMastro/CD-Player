// CD Player - Bounded TOC and CD-Text decoding, MusicBrainz identification.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/disc.hpp"
#define JSON_NOEXCEPTION 1
#include "third_party/nlohmann/json.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <map>

namespace cd
{
namespace
{
uint32_t be32(const uint8_t *p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
uint16_t be16(const uint8_t *p)
{
    return uint16_t((p[0] << 8) | p[1]);
}
void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        p[i] = uint8_t(v >> (24 - i * 8));
}
std::string text(const nlohmann::json &object, const char *field)
{
    auto it = object.find(field);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string{};
}
std::string credit(const nlohmann::json &j)
{
    std::string out;
    auto it = j.find("artist-credit");
    if (it == j.end() || !it->is_array())
        return out;
    for (const auto &a : *it)
    {
        std::string name = text(a, "name");
        if (name.empty() && a.contains("artist"))
            name = text(a["artist"], "name");
        out += name + text(a, "joinphrase");
    }
    return out;
}
bool uuid(std::string_view s)
{
    if (s.size() != 36)
        return false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (s[i] != '-')
                return false;
        }
        else if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') ||
                   (s[i] >= 'A' && s[i] <= 'F')))
            return false;
    }
    return true;
}
} // namespace
Command inquiry()
{
    Command c;
    c.bytes[0] = 0x12;
    c.bytes[4] = 96;
    c.allocation = 96;
    c.short_reply = true;
    return c;
}
Command ready()
{
    return {};
}
Command apple_wake()
{
    Command c;
    c.bytes[0] = 0xea;
    c.bytes[6] = 1;
    c.length = 7;
    return c;
}
Command unlock()
{
    Command c;
    c.bytes[0] = 0x1e;
    return c;
}
Command eject()
{
    Command c;
    c.bytes[0] = 0x1b;
    c.bytes[4] = 2;
    return c;
}
Command read_toc(bool cdtext)
{
    Command c;
    c.length = 10;
    c.bytes[0] = 0x43;
    c.bytes[2] = cdtext ? 5 : 0;
    c.allocation = cdtext ? 32768 : 804;
    c.bytes[7] = uint8_t(c.allocation >> 8);
    c.bytes[8] = uint8_t(c.allocation);
    c.short_reply = true;
    return c;
}
Command read_audio(uint32_t lba, unsigned sectors)
{
    Command c;
    c.length = 12;
    c.bytes[0] = 0xbe;
    c.bytes[1] = 4;
    put32(&c.bytes[2], lba);
    c.bytes[6] = uint8_t(sectors >> 16);
    c.bytes[7] = uint8_t(sectors >> 8);
    c.bytes[8] = uint8_t(sectors);
    c.bytes[9] = 0x10;
    c.allocation = sectors * sector_bytes;
    return c;
}
bool parse_toc(std::span<const uint8_t> data, Disc &out, std::string &error)
{
    auto bad = [&error](const char *why)
    {
        error = why;
        return false;
    };
    if (data.size() < 4)
        return bad("Incomplete TOC");
    const size_t size = size_t(be16(data.data())) + 2;
    if (size > data.size() || size < 20 || (size - 4) % 8 != 0)
        return bad("Invalid TOC length");
    Disc d;
    d.first = data[2];
    d.last = data[3];
    if (d.first < 1 || d.last > 99 || d.last < d.first)
        return bad("Invalid track numbers");
    if ((size - 4) / 8 != d.last - d.first + 2)
        return bad("Missing TOC tracks");
    uint32_t previous = 0;
    unsigned expected = d.first;
    for (size_t at = 4; at < size; at += 8)
    {
        const unsigned number = data[at + 2];
        const uint32_t lba = be32(data.data() + at + 4);
        if (lba > 450000 || (at > 4 && lba <= previous))
            return bad("Invalid track positions");
        if (number == 0xaa)
        {
            if (at + 8 != size || expected != d.last + 1)
                return bad("Lead-out out of order");
            d.leadout = lba;
        }
        else
        {
            if (number != expected || number > d.last)
                return bad("Invalid track order");
            Track t;
            t.number = number;
            t.control = data[at + 1] & 15;
            t.start = lba;
            char name[32];
            std::snprintf(name, sizeof(name), "Track %02u", number);
            t.title = name;
            d.tracks.push_back(std::move(t));
            ++expected;
        }
        previous = lba;
    }
    if (d.leadout == 0 || d.tracks.empty())
        return bad("Missing lead-out");
    bool audio = false;
    for (size_t i = 0; i < d.tracks.size(); ++i)
    {
        d.tracks[i].end = i + 1 < d.tracks.size() ? d.tracks[i + 1].start : d.leadout;
        audio |= d.tracks[i].audio();
    }
    if (!audio)
        return bad("This disc has no audio tracks");
    d.id = disc_id(d);
    out = std::move(d);
    error.clear();
    return true;
}
std::array<uint8_t, 20> sha1(std::span<const uint8_t> bytes)
{
    std::vector<uint8_t> data(bytes.begin(), bytes.end());
    const uint64_t bits = uint64_t(data.size()) * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56)
        data.push_back(0);
    for (int i = 7; i >= 0; --i)
        data.push_back(uint8_t(bits >> (i * 8)));
    std::array<uint32_t, 5> h = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    for (size_t block = 0; block < data.size(); block += 64)
    {
        std::array<uint32_t, 80> w{};
        for (unsigned i = 0; i < 16; ++i)
            w[i] = be32(&data[block + i * 4]);
        for (unsigned i = 16; i < 80; ++i)
            w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        auto [a, b, c, d, e] = h;
        for (unsigned i = 0; i < 80; ++i)
        {
            const uint32_t f = i < 20   ? (b & c) | (~b & d)
                               : i < 40 ? b ^ c ^ d
                               : i < 60 ? (b & c) | (b & d) | (c & d)
                                        : b ^ c ^ d;
            const uint32_t k = i < 20   ? 0x5a827999
                               : i < 40 ? 0x6ed9eba1
                               : i < 60 ? 0x8f1bbcdc
                                        : 0xca62c1d6;
            uint32_t next = std::rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = next;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }
    std::array<uint8_t, 20> digest{};
    for (unsigned i = 0; i < 5; ++i)
        put32(&digest[i * 4], h[i]);
    return digest;
}
std::string disc_id(const Disc &d)
{
    if (d.tracks.empty())
        return {};
    unsigned last_audio = 0;
    for (const auto &t : d.tracks)
        if (t.audio())
            last_audio = t.number;
    if (last_audio == 0)
        return {};
    uint32_t end = d.leadout;
    if (last_audio < d.last)
    {
        auto it = std::find_if(d.tracks.begin(), d.tracks.end(),
                               [last_audio](const auto &t) { return t.number == last_audio + 1; });
        if (it == d.tracks.end() || it->start < 11400)
            return {};
        end = it->start - 11400;
    }
    std::string text_id;
    char part[16];
    std::snprintf(part, sizeof(part), "%02X%02X%08X", d.first, last_audio, end + 150);
    text_id = part;
    for (unsigned n = 1; n <= 99; ++n)
    {
        uint32_t offset = 0;
        for (const auto &t : d.tracks)
            if (t.number == n && n <= last_audio)
                offset = t.start + 150;
        std::snprintf(part, sizeof(part), "%08X", offset);
        text_id += part;
    }
    auto hash = sha1({reinterpret_cast<const uint8_t *>(text_id.data()), text_id.size()});
    const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
    std::string out;
    uint32_t value = 0;
    unsigned bits = 0;
    for (auto b : hash)
    {
        value = (value << 8) | b;
        bits += 8;
        while (bits >= 6)
        {
            bits -= 6;
            out += alphabet[(value >> bits) & 63];
        }
    }
    if (bits)
        out += alphabet[(value << (6 - bits)) & 63];
    while (out.size() % 4)
        out += '-';
    return out;
}
bool parse_cdtext(std::span<const uint8_t> data, Disc &disc)
{
    if (data.size() < 4)
        return false;
    const size_t end = size_t(be16(data.data())) + 2;
    if (end > data.size() || (end - 4) % 18)
        return false;
    std::map<std::pair<unsigned, unsigned>, std::string> values;
    bool found = false;
    for (size_t at = 4; at < end; at += 18)
    {
        const auto *pack = data.data() + at;
        // Only single-byte block zero is decoded; unsupported encodings remain online fallbacks.
        if ((pack[3] & 0xf0) != 0 || (pack[0] != 0x80 && pack[0] != 0x81))
            continue;
        uint16_t crc = 0;
        for (unsigned i = 0; i < 16; ++i)
        {
            crc ^= uint16_t(pack[i]) << 8;
            for (unsigned b = 0; b < 8; ++b)
                crc = uint16_t((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
        }
        if (uint16_t(~crc) != be16(pack + 16))
            continue;
        unsigned track = pack[1];
        for (unsigned i = 4; i < 16 && track <= 99; ++i)
        {
            const uint8_t c = pack[i];
            if (c == 0)
            {
                ++track;
                continue;
            }
            auto &value = values[{pack[0], track}];
            if (value.size() >= 1024)
                continue;
            if (c < 0x80)
                value += char(c);
            else
            {
                value += char(0xc0 | (c >> 6));
                value += char(0x80 | (c & 63));
            }
        }
        found = true;
    }
    if (!values[{0x80, 0}].empty())
        disc.album = values[{0x80, 0}];
    disc.artist = values[{0x81, 0}];
    for (auto &t : disc.tracks)
    {
        if (!values[{0x80, t.number}].empty())
            t.title = values[{0x80, t.number}];
        t.artist = values[{0x81, t.number}].empty() ? disc.artist : values[{0x81, t.number}];
    }
    return found;
}
std::vector<Release> parse_releases(std::string_view bytes, const Disc &disc)
{
    auto j = nlohmann::json::parse(bytes, nullptr, false);
    std::vector<Release> out;
    if (j.is_discarded() || !j.is_object() || !j.contains("releases") || !j["releases"].is_array())
        return out;
    for (const auto &release : j["releases"])
    {
        if (!release.is_object() || !release.contains("media") || !release["media"].is_array())
            continue;
        unsigned medium_position=0;
        for (const auto &medium : release["media"])
        {
            ++medium_position;
            bool matches = false;
            if (medium.contains("discs") && medium["discs"].is_array())
                for (const auto &d : medium["discs"])
                    if (text(d, "id") == disc.id)
                        matches = true;
            if (!matches || !medium.contains("tracks") || !medium["tracks"].is_array())
                continue;
            Release r;
            r.medium_position=medium_position;
            if (medium.contains("position") && medium["position"].is_number_unsigned() &&
                medium["position"].get<unsigned>() > 0 && medium["position"].get<unsigned>() <= 100)
                r.medium_position=medium["position"].get<unsigned>();
            r.medium_count=unsigned(release["media"].size());
            r.id = text(release, "id");
            r.album = text(release, "title");
            r.artist = credit(release);
            r.date = text(release, "date");
            r.country = text(release, "country");
            r.description = text(release, "disambiguation");
            for (const auto &track : medium["tracks"])
            {
                if (!track.is_object() || !track.contains("position") ||
                    !track["position"].is_number_unsigned())
                    continue;
                Track t;
                t.number = track["position"].get<unsigned>();
                t.title = text(track, "title");
                t.artist = credit(track);
                if (t.artist.empty())
                    t.artist = r.artist;
                r.tracks.push_back(std::move(t));
            }
            if (uuid(r.id) && !r.album.empty() && r.tracks.size() == disc.tracks.size())
                out.push_back(std::move(r));
            break;
        }
        if (out.size() >= 50)
            break;
    }
    return out;
}
bool apply_release(const Release &r, Disc &d)
{
    if (r.tracks.size() != d.tracks.size())
        return false;
    for (size_t i = 0; i < d.tracks.size(); ++i)
        if (d.tracks[i].number != r.tracks[i].number)
            return false;
    d.album = r.album;
    d.artist = r.artist;
    d.year = r.date.substr(0, 4);
    d.release = r.id;
    d.medium_position=r.medium_position;
    d.medium_count=r.medium_count;
    for (size_t i = 0; i < d.tracks.size(); ++i)
    {
        d.tracks[i].title = r.tracks[i].title;
        d.tracks[i].artist = r.tracks[i].artist;
    }
    return true;
}
} // namespace cd
