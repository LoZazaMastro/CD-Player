// CD Player - Transport, metadata and signal regressions.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/disc.hpp"
#include "cd/sound.hpp"
#include "cd/dsp.hpp"
#include "cd/lyrics.hpp"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numbers>
#include <random>
#include <limits>
#include <source_location>

int main()
{
    unsigned checks = 0;
    auto check = [&](bool value, std::source_location location = std::source_location::current())
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Check %u failed at %s:%u\n", checks, location.file_name(), location.line());
            std::abort();
        }
    };
    const std::string abc = "abc";
    const auto hash = cd::sha1({reinterpret_cast<const uint8_t *>(abc.data()), abc.size()});
    const std::array<uint8_t, 20> sha_expected = {0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81,
                                                  0x6a, 0xba, 0x3e, 0x25, 0x71, 0x78, 0x50,
                                                  0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d};
    check(hash == sha_expected);
    cd::Disc disc;
    disc.first = 1;
    disc.last = 10;
    disc.leadout = 298363;
    const unsigned starts[] = {0,      28731,  53538,  79855,  109931,
                               127902, 155653, 205953, 239008, 268147};
    for (unsigned i = 0; i < 10; ++i)
    {
        cd::Track t;
        t.number = i + 1;
        t.start = starts[i];
        t.end = i < 9 ? starts[i + 1] : disc.leadout;
        disc.tracks.push_back(t);
    }
    check(cd::disc_id(disc) == "_GDJVYrAWd5Jcxqg32msRDz7aA4-");
    disc.id = cd::disc_id(disc);
    std::ifstream f("tests/fixtures/reference-disc.musicbrainz.json");
    std::string metadata((std::istreambuf_iterator<char>(f)), {});
    const auto releases = cd::parse_releases(metadata, disc);
    check(releases.size() == 4);
    check(cd::apply_release(releases[0], disc));
    check(disc.album == "A Deeper Understanding");
    check(disc.year == "2017");
    check(disc.artist == "The War on Drugs");
    check(!cd::parse_releases("[]", disc).size());
    check(!cd::parse_releases("{", disc).size());
    auto wrong = releases[0];
    wrong.tracks[1].number = 99;
    check(!cd::apply_release(wrong, disc));
    auto cdb = cd::read_audio(0x12345678, 20);
    check(cdb.length == 12 && cdb.bytes[0] == 0xbe && cdb.bytes[1] == 4 && cdb.bytes[5] == 0x78 &&
          cdb.bytes[8] == 20 && cdb.bytes[9] == 0x10 && cdb.allocation == 47040);
    check(cd::apple_wake().bytes[6] == 1 && cd::apple_wake().length == 7);
    check(cd::eject().bytes[4] == 2);
    check(cd::unlock().bytes[0] == 0x1e);
    std::vector<uint8_t> toc(92);
    toc[1] = 90;
    toc[2] = 1;
    toc[3] = 10;
    for (unsigned i = 0; i < 11; ++i)
    {
        const unsigned at = 4 + i * 8, lba = i < 10 ? starts[i] : 298363;
        toc[at + 1] = 0x10;
        toc[at + 2] = uint8_t(i < 10 ? i + 1 : 170);
        for (unsigned b = 0; b < 4; ++b)
            toc[at + 4 + b] = uint8_t(lba >> (24 - b * 8));
    }
    cd::Disc parsed;
    std::string error;
    check(cd::parse_toc(toc, parsed, error));
    check(parsed.id == disc.id);
    auto invalid = toc;
    invalid[14] = 3;
    check(!cd::parse_toc(invalid, parsed, error));
    for (size_t length = 0; length < toc.size(); ++length)
        check(!cd::parse_toc(std::span(toc).first(length), parsed, error));
    std::mt19937 random(773);
    for (unsigned n = 0; n < 3000; ++n)
    {
        std::vector<uint8_t> bytes(random() % 1200);
        for (auto &b : bytes)
            b = uint8_t(random());
        cd::parse_toc(bytes, parsed, error);
        cd::parse_cdtext(bytes, parsed);
    }
    std::vector<uint8_t> pcm(44100 * 4);
    for (unsigned i = 0; i < 44100; ++i)
    {
        const int16_t sample = int16_t(std::sin(2. * std::numbers::pi * 1000 * i / 44100) * 12000);
        pcm[i * 4] = pcm[i * 4 + 2] = uint8_t(sample);
        pcm[i * 4 + 1] = pcm[i * 4 + 3] = uint8_t(uint16_t(sample) >> 8);
    }
    cd::Resampler all, split;
    all.reset();
    split.reset();
    const auto a = all.push(pcm, true);
    std::vector<float> b;
    for (size_t at = 0; at < pcm.size();)
    {
        const size_t count = std::min(size_t(588 * 4), pcm.size() - at);
        auto block = split.push(std::span(pcm).subspan(at, count), at + count == pcm.size());
        b.insert(b.end(), block.begin(), block.end());
        at += count;
    }
    check(a.size() == 96000);
    check(a.size() == b.size());
    float difference = 0;
    for (size_t i = 0; i < a.size(); ++i)
        difference = std::max(difference, std::abs(a[i] - b[i]));
    check(difference < 1e-6f);
    double rms = 0;
    for (size_t i = 128; i < 47000; ++i)
        rms += double(a[i * 2]) * a[i * 2];
    rms = std::sqrt(rms / (47000 - 128));
    check(std::abs(rms - 12000. / 32768 / std::sqrt(2.)) < .001);
    cd::AudioRing ring;
    check(ring.write(std::span(a).first(1024), 1, 100));
    cd::AudioFrame frame;
    check(!ring.read(frame, 1, false));
    check(ring.available() == 512);
    check(ring.read(frame, 1, true));
    check(frame.position == 100);
    check(!ring.read(frame, 2, false));
    check(ring.available() == 0);
    check(ring.write(std::span(a).first(1024), 2, 200));
    check(ring.read(frame, 2, true));
    check(frame.position == 200);
    cd::Dsp dsp;
    std::vector<int16_t> stereo(1024), out(4096);
    for (auto &sample : stereo)
        sample = int16_t(random());
    dsp.process(stereo, out, 8);
    for (unsigned i = 0; i < 512; ++i)
    {
        check(out[i * 8] == stereo[i * 2] && out[i * 8 + 1] == stereo[i * 2 + 1]);
        for (unsigned c = 2; c < 8; ++c)
            check(out[i * 8 + c] == 0);
    }
    dsp.equalizer(true);
    dsp.band(5, 6);
    for (size_t i = 0; i < 512; ++i)
        stereo[i * 2] = stereo[i * 2 + 1] =
            int16_t(3000 * std::sin(2. * std::numbers::pi * 1000 * i / 48000));
    for (unsigned n = 0; n < 100; ++n)
        dsp.process(stereo, out, 8);
    double input_energy = 0, output_energy = 0;
    for (size_t i = 0; i < 512; ++i)
    {
        input_energy += double(stereo[i * 2]) * stereo[i * 2];
        output_energy += double(out[i * 8]) * out[i * 8];
    }
    check(std::sqrt(output_energy / input_energy) > 1.8 &&
          std::sqrt(output_energy / input_energy) < 2.2);
    dsp.upmix(false);
    dsp.equalizer(false);
    for (unsigned n = 0; n < 20; ++n)
        dsp.process(stereo, out, 8);
    for (unsigned i = 0; i < 512; ++i)
        check(out[i * 8] == stereo[i * 2] && out[i * 8 + 1] == stereo[i * 2 + 1]);
    cd::Dsp matrix;
    matrix.upmix(true);
    std::vector<int16_t> silence(1024), channels_out(4096);
    for (unsigned i = 0; i < 20; ++i)
        matrix.process(silence, channels_out, 8);
    std::vector<int16_t> impulse(4096), surround(16384);
    impulse[0] = 12000;
    matrix.process(impulse, surround, 8);
    check(surround[0] == 9600);
    check(surround[2] > 0 && surround[3] > 0);
    check(surround[720 * 8 + 6] > 0 && surround[720 * 8 + 7] < 0);
    check(surround[1200 * 8 + 4] > 0 && surround[1200 * 8 + 5] < 0);
    // Independent generated-channel gains, preserving the original front pair.
    std::vector<int16_t> signal(48000*2), normal(48000*8), louder(48000*8), bass(48000*8);
    for (size_t i=0;i<48000;++i)
    {
        signal[i*2]=int16_t(900*std::sin(2*std::numbers::pi*70*i/48000));
        signal[i*2+1]=int16_t(600*std::sin(2*std::numbers::pi*110*i/48000));
    }
    cd::Dsp reference, boosted, sub;
    reference.upmix(true);boosted.upmix(true);sub.upmix(true);
    for(size_t c : {2u,4u,5u,6u,7u}) boosted.channel_gain(c,8);
    sub.channel_gain(3,8);
    reference.process(signal,normal,8);boosted.process(signal,louder,8);sub.process(signal,bass,8);
    for(size_t i=0;i<48000;++i)
        check(normal[i*8]==louder[i*8] && normal[i*8+1]==louder[i*8+1] &&
              normal[i*8]==bass[i*8] && normal[i*8+1]==bass[i*8+1]);
    for(unsigned c=0;c<8;++c)
    {
        double base=0,surround_energy=0,bass_energy=0;
        for(size_t i=12000;i<48000;++i)
        {
            base+=double(normal[i*8+c])*normal[i*8+c];
            surround_energy+=double(louder[i*8+c])*louder[i*8+c];
            bass_energy+=double(bass[i*8+c])*bass[i*8+c];
        }
        const double expected_surround=c>=2&&c!=3?8:1, expected_bass=c==3?8:1;
        check(base>1000 && std::abs(std::sqrt(surround_energy/base)-expected_surround)<.02);
        check(std::abs(std::sqrt(bass_energy/base)-expected_bass)<.02);
    }
    for(size_t c=2;c<8;++c) boosted.channel_gain(c,0);
    boosted.process(signal,louder,8);
    for(size_t i=12000;i<48000;++i)
        for(unsigned c=2;c<8;++c)check(louder[i*8+c]==0);
    for(size_t c=2;c<8;++c) boosted.channel_gain(c,1000);
    check(boosted.channel_gain(2)==cd::Dsp::maximum_gain && boosted.channel_gain(3)==cd::Dsp::maximum_gain);
    boosted.channel_gain(2,std::numeric_limits<float>::infinity());
    boosted.channel_gain(3,std::numeric_limits<float>::quiet_NaN());
    check(boosted.channel_gain(2)==cd::Dsp::maximum_gain && boosted.channel_gain(3)==cd::Dsp::maximum_gain);
    std::fill(signal.begin(),signal.end(),32767);boosted.process(signal,louder,8);
    check(louder[louder.size()-6]>32000 && louder[louder.size()-5]>32000);
    check(louder[louder.size()-8]==26214 && louder[louder.size()-7]==26214);
    std::vector<int16_t> stereo_only(signal.size());
    boosted.process(signal,stereo_only,2);
    // The preceding 8-channel stream crossfades out of its existing upmix.
    boosted.process(signal,stereo_only,2);
    check(signal==stereo_only);
    // Every output gain affects exactly one speaker, including the front pair.
    for(size_t i=0;i<48000;++i)
    {
        signal[i*2]=int16_t(900*std::sin(2*std::numbers::pi*70*i/48000));
        signal[i*2+1]=int16_t(600*std::sin(2*std::numbers::pi*110*i/48000));
    }
    for(size_t channel=0;channel<8;++channel)
    {
        cd::Dsp individual;individual.upmix(true);individual.channel_gain(channel,2);
        individual.process(signal,louder,8);
        for(size_t c=0;c<8;++c)
        {
            double before=0,after=0;
            for(size_t i=12000;i<48000;++i)
            {
                before+=double(normal[i*8+c])*normal[i*8+c];
                after+=double(louder[i*8+c])*louder[i*8+c];
                if(c!=channel)check(normal[i*8+c]==louder[i*8+c]);
            }
            check(std::abs(std::sqrt(after/before)-(c==channel?2:1))<.02);
        }
        individual.channel_gain(channel,0);individual.process(signal,louder,8);
        for(size_t i=12000;i<48000;++i)check(louder[i*8+channel]==0);
    }
    cd::Dsp stereo_balance;stereo_balance.channel_gain(0,.5f);
    stereo_balance.process(signal,stereo_only,2);
    for(size_t i=12000;i<48000;++i)
        check(std::abs(int(stereo_only[i*2])*2-int(signal[i*2]))<=1 && stereo_only[i*2+1]==signal[i*2+1]);
    // Timestamp parsing and matching use CD playback time, including backward seeks.
    const auto lyrics=cd::parse_lrc("[ar:Test]\n[00:01.50]First phrase\n[00:04.000][00:12.00]Repeated phrase\n[00:07]Third phrase");
    check(lyrics.synced && lyrics.lines.size()==4);
    check(cd::lyric_line_at(lyrics,1.49)==-1 && cd::lyric_line_at(lyrics,1.5)==0);
    check(cd::lyric_line_at(lyrics,100)==3 && cd::lyric_line_at(lyrics,5)==1);
    check(cd::lyric_line_at(lyrics,std::numeric_limits<double>::quiet_NaN())==-1);
    const auto offset=cd::parse_lrc("\xef\xbb\xbf[offset:500]\n[00:01.00]<00:01.00>One <00:01.25>two");
    check(offset.lines.size()==1 && offset.lines[0].seconds==.5);
    check(offset.lines[0].words.size()==2 && offset.lines[0].words[1].seconds==.75);
    check(offset.lines[0].text=="One two");
    const auto bad=cd::parse_lrc("[99:99]Bad\n[-1:04]Bad\n[0:1.1234]Bad\n[0:2.1x]Bad\n[0:05]Good");
    check(bad.lines.size()==1 && bad.lines[0].text=="Good");
    check(cd::parse_lrc(std::string(256*1024+1,'x')).lines.empty());
    cd::Track song;song.title="Test Song";song.start=0;song.end=75*200;
    cd::Disc record;record.artist="Test Artist";record.album="Test Album";
    const std::string response=R"({"trackName":"Test Song","artistName":"Test Artist","duration":200,"syncedLyrics":"[00:01.00]A test phrase\n[00:02.00]Another test phrase"})";
    check(cd::parse_lyrics_response(response,song,record).synced);
    song.end=75*210;check(cd::parse_lyrics_response(response,song,record).lines.empty());song.end=75*200;
    song.title="Other Song";check(cd::parse_lyrics_response(response,song,record).lines.empty());song.title="Test Song";
    const auto plain=cd::parse_lyrics_response(R"({"trackName":"Test Song","artistName":"Test Artist","duration":200,"plainLyrics":"A test phrase\nAnother phrase"})",song,record);
    check(!plain.synced && plain.lines.size()==2 && cd::lyric_line_at(plain,10)==-1);
    check(cd::parse_lyrics_response(R"({"trackName":"Test Song","artistName":"Test Artist","duration":200,"instrumental":true})",song,record).instrumental);
    check(cd::parse_lyrics_response("{",song,record).lines.empty());
    check(cd::lyrics_url(song,record).find("track_name=Test%20Song")!=std::string::npos);
    const auto cache=cd::lyrics_cache_key(song,record);song.end+=75;
    check(cache.starts_with("lyrics-") && cache!=cd::lyrics_cache_key(song,record));
    for(unsigned n=0;n<1000;++n)
    {
        std::string junk(random()%1000,' ');for(auto &c:junk)c=char(random());
        cd::parse_lrc(junk);cd::parse_lyrics_response(junk,song,record);
    }
    cd::Analyzer analyzer;
    for (unsigned i = 0; i < 2048; ++i)
        analyzer.feed(std::sin(2.f * std::numbers::pi_v<float> * 1000 * i / 48000), 0);
    auto bands = analyzer.bands();
    check(*std::max_element(bands.begin(), bands.end()) > .1f);
    std::printf("%u checks passed; 3000 malformed TOC/CD-Text cases; chunk-invariant resampling; "
                "EQ/upmix transitions\n",
                checks);
}
