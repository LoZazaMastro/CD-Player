// CD Player - LRCLIB metadata matching, LRC and enhanced LRC timing.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/lyrics.hpp"
#define JSON_NOEXCEPTION 1
#include "third_party/nlohmann/json.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>

namespace cd
{
namespace
{
std::string trim(std::string_view s)
{
    while (!s.empty() && (s.front()==' ' || s.front()=='\t' || s.front()=='\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back()==' ' || s.back()=='\t' || s.back()=='\r')) s.remove_suffix(1);
    return std::string(s);
}
std::string canonical(std::string_view s)
{
    std::string result;
    for (unsigned char c:s)
    {
        if (c>='A' && c<='Z') c+=32;
        if ((c>='a' && c<='z') || (c>='0' && c<='9') || c>=128) result+=char(c);
    }
    return result;
}
bool timestamp(std::string_view s,double &out)
{
    const auto colon=s.find(':');
    if (colon==std::string_view::npos || colon==0 || colon>3) return false;
    unsigned minutes=0,seconds=0;
    const auto a=std::from_chars(s.data(),s.data()+colon,minutes);
    if (a.ec!=std::errc{} || a.ptr!=s.data()+colon || minutes>240) return false;
    s.remove_prefix(colon+1);
    const auto dot=s.find_first_of(".:");
    const auto whole=s.substr(0,dot);
    if (whole.empty() || whole.size()>2) return false;
    const auto b=std::from_chars(whole.data(),whole.data()+whole.size(),seconds);
    if (b.ec!=std::errc{} || b.ptr!=whole.data()+whole.size() || seconds>=60) return false;
    out=double(minutes*60+seconds);
    if (dot!=std::string_view::npos)
    {
        const auto fraction=s.substr(dot+1);
        if (fraction.empty() || fraction.size()>3) return false;
        double scale=.1;
        for (char c:fraction) { if(c<'0'||c>'9')return false;out+=(c-'0')*scale;scale*=.1; }
    }
    return true;
}
std::string field(const nlohmann::json &j,const char *key)
{
    const auto it=j.find(key);return it!=j.end()&&it->is_string()?it->get<std::string>():std::string{};
}
} // namespace
Lyrics parse_lrc(std::string_view text)
{
    Lyrics result;
    if (text.size()>256u*1024) return result;
    if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    double offset=0;
    for(size_t at=0;at<text.size() && result.lines.size()<2048;)
    {
        size_t end=text.find('\n',at);if(end==std::string_view::npos)end=text.size();
        std::string_view line=text.substr(at,end-at);at=end+1;
        if(line.size()>8192)continue;
        if(line.starts_with("[offset:"))
        {
            const auto close=line.find(']');
            if(close!=std::string_view::npos)
            {
                int ms=0;const auto number=line.substr(8,close-8);
                const auto p=std::from_chars(number.data(),number.data()+number.size(),ms);
                if(p.ec==std::errc{} && p.ptr==number.data()+number.size() && std::abs(double(ms))<=60000)offset=double(ms)/1000;
            }
            continue;
        }
        std::vector<double> starts;
        while(line.starts_with('[') && starts.size()<32)
        {
            const auto close=line.find(']');double seconds=0;
            if(close==std::string_view::npos || !timestamp(line.substr(1,close-1),seconds))break;
            starts.push_back(seconds);line.remove_prefix(close+1);
        }
        if(starts.empty())continue;
        LyricLine decoded;double word_time=-1;std::string word;
        const auto flush=[&](){if(!word.empty()){decoded.text+=word;if(word_time>=0)decoded.words.push_back({word_time,word});word.clear();}};
        for(size_t i=0;i<line.size();)
        {
            if(line[i]=='<')
            {
                const auto close=line.find('>',i);double seconds=0;
                if(close!=std::string_view::npos && timestamp(line.substr(i+1,close-i-1),seconds))
                {flush();word_time=seconds;i=close+1;continue;}
            }
            word+=line[i++];
        }
        flush();decoded.text=trim(decoded.text);
        if(!std::is_sorted(decoded.words.begin(),decoded.words.end(),[](const auto &a,const auto &b){return a.seconds<b.seconds;}))decoded.words.clear();
        for(double start:starts)
        {
            auto copy=decoded;copy.seconds=start;
            for(auto &w:copy.words)w.seconds+=start-starts.front();
            result.lines.push_back(std::move(copy));
            if(result.lines.size()>=2048)break;
        }
    }
    for(auto &line:result.lines)
    {
        line.seconds=std::max(0.,line.seconds-offset);
        for(auto &word:line.words)word.seconds=std::max(0.,word.seconds-offset);
    }
    std::stable_sort(result.lines.begin(),result.lines.end(),[](const auto &a,const auto &b){return a.seconds<b.seconds;});
    result.synced=!result.lines.empty();return result;
}
int lyric_line_at(const Lyrics &lyrics,double seconds)
{
    if(!lyrics.synced || !std::isfinite(seconds))return -1;
    const auto it=std::upper_bound(lyrics.lines.begin(),lyrics.lines.end(),seconds,[](double time,const auto &line){return time<line.seconds;});
    return it==lyrics.lines.begin()?-1:int(it-lyrics.lines.begin())-1;
}
Lyrics parse_lyrics_response(std::string_view bytes,const Track &track,const Disc &disc)
{
    Lyrics result;
    if(bytes.size()>512u*1024)return result;
    const auto j=nlohmann::json::parse(bytes,nullptr,false);
    if(!j.is_object())return result;
    const auto artist=track.artist.empty()?disc.artist:track.artist;
    const auto duration=j.find("duration");
    if(canonical(field(j,"trackName"))!=canonical(track.title) || canonical(field(j,"artistName"))!=canonical(artist) ||
       duration==j.end() || !duration->is_number() || !std::isfinite(duration->get<double>()) ||
       track.end<=track.start || std::abs(duration->get<double>()-double(track.end-track.start)/75)>2.1)return result;
    if(const auto flag=j.find("instrumental");flag!=j.end()&&flag->is_boolean()&&flag->get<bool>())
    {result.instrumental=true;return result;}
    result=parse_lrc(field(j,"syncedLyrics"));
    if(result.synced)return result;
    const auto plain=field(j,"plainLyrics");
    if(plain.size()>256u*1024)return result;
    for(size_t at=0;at<plain.size() && result.lines.size()<2048;)
    {
        auto end=plain.find('\n',at);if(end==std::string::npos)end=plain.size();
        auto line=trim(std::string_view(plain).substr(at,end-at));at=end+1;
        if(!line.empty()&&line.size()<=8192)result.lines.push_back({0,std::move(line),{}});
    }
    return result;
}
std::string lyrics_url(const Track &track,const Disc &disc)
{
    if(track.title.empty() || (track.artist.empty()&&disc.artist.empty()) || track.end<=track.start)return {};
    return "https://lrclib.net/api/get?track_name="+url_encode(track.title)+"&artist_name="+
        url_encode(track.artist.empty()?disc.artist:track.artist)+"&album_name="+url_encode(disc.album)+
        "&duration="+std::to_string((track.end-track.start+37)/75);
}
std::string lyrics_cache_key(const Track &track,const Disc &disc)
{
    const auto key=lyrics_url(track,disc);if(key.empty())return {};
    const auto hash=sha1({reinterpret_cast<const uint8_t*>(key.data()),key.size()});
    std::string result="lyrics-";char byte[3];
    for(auto b:hash){std::snprintf(byte,sizeof(byte),"%02x",b);result+=byte;}
    return result;
}
} // namespace cd
