// CD Player - Descriptions tied to the identified MusicBrainz album.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/disc.hpp"
#define JSON_NOEXCEPTION 1
#include "third_party/nlohmann/json.hpp"
#include <algorithm>

namespace cd
{
namespace
{
using Json = nlohmann::json;
std::string field(const Json &value, const char *key)
{
    if (!value.is_object()) return {};
    auto it=value.find(key);
    return it!=value.end() && it->is_string() ? it->get<std::string>() : std::string{};
}
std::string description(std::string value)
{
    if(value.size()>16000)
    {
        size_t end=16000;
        while(end>0 && (uint8_t(value[end])&0xc0)==0x80) --end;
        value.resize(end);
    }
    value.erase(std::remove(value.begin(),value.end(),'\r'),value.end());
    for(size_t at=0;(at=value.find("''",at))!=std::string::npos;) value.erase(at,2);
    // MusicBrainz annotations use [url|label] wiki links. Keep their label.
    for(size_t at=0;(at=value.find('[',at))!=std::string::npos;)
    {
        size_t end=value.find(']',at),pipe=value.find('|',at);
        if(end==std::string::npos) break;
        if(pipe!=std::string::npos && pipe<end)
            value.replace(at,end-at+1,value.substr(pipe+1,end-pipe-1));
        else ++at;
    }
    if(value.find_first_not_of(" \t\n")==std::string::npos) return {};
    return value;
}
bool group_id(std::string_view value)
{
    if(value.size()!=36) return false;
    for(size_t i=0;i<value.size();++i)
    {
        if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')||(value[i]>='A'&&value[i]<='F')))return false;
    }
    return true;
}
}
AlbumInfo parse_album_info(std::string_view release_json,std::string_view group_json,std::string_view release_id)
{
    AlbumInfo info;
    auto release=Json::parse(release_json,nullptr,false);
    if(!release.is_object() || field(release,"id")!=release_id) return info;
    info.annotation=description(field(release,"annotation"));
    info.date=field(release,"date");info.country=field(release,"country");
    if(release.contains("release-group") && release["release-group"].is_object())
    {
        const auto &group=release["release-group"];
        info.group_id=field(group,"id");
        if(!group_id(info.group_id)) info.group_id.clear();
        info.type=field(group,"primary-type");info.first_date=field(group,"first-release-date");
    }
    if(release.contains("label-info") && release["label-info"].is_array())
        for(const auto &label:release["label-info"])
            if(label.is_object() && label.contains("label"))
            {
                const auto name=field(label["label"],"name");
                if(name.empty() || info.labels.size()>1024) continue;
                if(!info.labels.empty()) info.labels+=" / ";
                info.labels+=name;
            }
    auto group=Json::parse(group_json,nullptr,false);
    if(!info.group_id.empty() && group.is_object() && field(group,"id")==info.group_id)
    {
        if(info.annotation.empty()) info.annotation=description(field(group,"annotation"));
        if(group.contains("relations") && group["relations"].is_array())
            for(const auto &relation:group["relations"])
            {
                if(field(relation,"type")!="wikidata" || !relation.contains("url")) continue;
                const auto url=field(relation["url"],"resource");
                constexpr std::string_view prefix="https://www.wikidata.org/wiki/";
                if(url.rfind(prefix,0)!=0) continue;
                const auto id=url.substr(prefix.size());
                if(id.size()>1 && id.size()<20 && id[0]=='Q' &&
                    std::all_of(id.begin()+1,id.end(),[](char c){return c>='0'&&c<='9';}))info.wikidata_id=id;
            }
    }
    if(!info.annotation.empty()){info.source="MusicBrainz";info.source_url="https://musicbrainz.org/release/"+std::string(release_id);}
    return info;
}
std::string parse_wikipedia_title(std::string_view json,std::string_view id)
{
    auto j=Json::parse(json,nullptr,false);
    if(!j.is_object() || !j.contains("entities") || !j["entities"].is_object() || !j["entities"].contains(id))return {};
    const auto &entity=j["entities"][id];
    if(!entity.is_object() || field(entity,"id")!=id || !entity.contains("sitelinks"))return {};
    const auto &links=entity["sitelinks"];
    if(!links.is_object() || !links.contains("enwiki"))return {};
    auto title=field(links["enwiki"],"title");return title.size()<=512?title:std::string{};
}
std::string parse_wikipedia_description(std::string_view json)
{
    auto j=Json::parse(json,nullptr,false);
    if(!j.is_object() || field(j,"type")!="standard")return {};
    return description(field(j,"extract"));
}
std::string url_encode(std::string_view value)
{
    constexpr char digits[]="0123456789ABCDEF";
    std::string encoded;
    for(unsigned char c:value)
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')encoded+=char(c);
        else{encoded+='%';encoded+=digits[c>>4];encoded+=digits[c&15];}
    return encoded;
}
} // namespace cd
