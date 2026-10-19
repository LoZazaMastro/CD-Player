// CD Player - Bounded Cover Art Archive selection and image preparation.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cd/artwork.hpp"
#include "third_party/stb/stb_image.h"
#define JSON_NOEXCEPTION 1
#include "third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>

namespace cd
{
ArtworkSources parse_artwork_sources(std::string_view bytes,const std::string &release,
                                     unsigned medium,unsigned medium_count)
{
    ArtworkSources out;
    if(bytes.size()>(2u<<20) || release.size()!=36)return out;
    for(size_t i=0;i<release.size();++i)
        if(i==8||i==13||i==18||i==23 ? release[i]!='-' :
            !((release[i]>='0'&&release[i]<='9')||(release[i]>='a'&&release[i]<='f')))return out;
    const auto j=nlohmann::json::parse(bytes,nullptr,false);
    if(!j.is_object() || !j.contains("images") || !j["images"].is_array())return out;
    if(j.contains("release") && j["release"].is_string())
    {
        const auto source=j["release"].get<std::string>();
        if(source!="https://musicbrainz.org/release/"+release && source!="http://musicbrainz.org/release/"+release)return out;
    }
    int disc_score=-1,back_score=-1,spine_score=-1;
    unsigned visited=0;
    for(const auto &image:j["images"])
    {
        if(++visited>200)break;
        if(!image.is_object() || !image.contains("types") || !image["types"].is_array())continue;
        const auto type=[&](const char *name){return std::find(image["types"].begin(),image["types"].end(),name)!=image["types"].end();};
        if(type("Matrix/Runout"))continue;
        std::string id;
        if(image.contains("id") && image["id"].is_string())id=image["id"].get<std::string>();
        else if(image.contains("id") && image["id"].is_number_unsigned())id=std::to_string(image["id"].get<uint64_t>());
        if(id.empty() || id.size()>24 || id.find_first_not_of("0123456789")!=std::string::npos)continue;
        const auto url="https://coverartarchive.org/release/"+release+"/"+id+"-1200.jpg";
        int score=10;
        if(image.contains("approved") && image["approved"].is_boolean() && image["approved"].get<bool>())score+=10;
        if(type("Watermark"))score-=2;
        if(type("Raw/Unedited"))score-=1;
        if(type("Back") && score>back_score){out.back=url;out.back_has_spines=type("Spine");back_score=score;}
        if(type("Spine") && score>spine_score)
        {out.spine=url;out.combined_spine=type("Back");spine_score=score;}
        if(!type("Medium"))continue;
        if(medium_count>1 || medium>1)
        {
            if(!image.contains("comment") || !image["comment"].is_string())continue;
            auto comment=image["comment"].get<std::string>();
            for(auto &c:comment)if(c>='A'&&c<='Z')c+=32;
            bool matches=false;
            for(const char *prefix:{"disc ","cd ","disk "})
            {
                const auto marker=std::string(prefix)+std::to_string(medium);
                const auto at=comment.find(marker);
                if(at!=std::string::npos && (at+marker.size()==comment.size() ||
                    comment[at+marker.size()]<'0'||comment[at+marker.size()]>'9'))matches=true;
            }
            if(!matches)continue;
        }
        if(score>disc_score){out.disc=url;disc_score=score;}
    }
    return out;
}

std::shared_ptr<const ArtworkImage> decode_artwork(std::span<const uint8_t> bytes,ArtworkCrop crop)
{
    if(bytes.empty() || bytes.size()>(8u<<20))return {};
    int width=0,height=0,channels=0;
    if(!stbi_info_from_memory(bytes.data(),int(bytes.size()),&width,&height,&channels) ||
        width<2 || height<2 || width>4096 || height>4096 ||
        (crop!=ArtworkCrop::spine && (width<32 || height<32)))return {};
    auto *pixels=stbi_load_from_memory(bytes.data(),int(bytes.size()),&width,&height,&channels,4);
    if(!pixels)return {};
    const std::unique_ptr<uint8_t,void(*)(void*)> owner(pixels,stbi_image_free);
    int x=0,y=0,w=width,h=height;
    if(crop==ArtworkCrop::disc)
    {
        if(width>height*1.4 || height>width*1.4)return {};
        w=h=std::min(width,height);x=(width-w)/2;y=(height-h)/2;
    }
    if(crop==ArtworkCrop::back_with_spines || crop==ArtworkCrop::spine_from_back)
    {
        // Tray inlays are wider than they are tall, even after the folded
        // strips are removed. A square crop would cut off printed text.
        const bool tray=width>height*1.1 && width<=height*1.4;
        const int side=std::max(2,int(std::lround(width*.045)));
        if(crop==ArtworkCrop::spine_from_back)
        {if(!tray)return {};w=side;}
        else if(tray){x=side;w=width-side*2;}
    }
    const bool rotate=crop==ArtworkCrop::spine && w>h*3;
    const float shrink=std::min(1.f,1200.f/float(std::max(w,h)));
    auto image=std::make_shared<ArtworkImage>();
    image->width=std::max(1,int(float(rotate?h:w)*shrink));
    image->height=std::max(1,int(float(rotate?w:h)*shrink));
    image->rgba.resize(size_t(image->width)*size_t(image->height)*4);
    for(int row=0;row<image->height;++row)for(int col=0;col<image->width;++col)
    {
        const float u=(col+.5f)/image->width,v=(row+.5f)/image->height;
        const float sx=x+(rotate?v:u)*w-.5f,sy=y+(rotate?1-u:v)*h-.5f;
        const int x0=std::clamp(int(std::floor(sx)),0,width-1),y0=std::clamp(int(std::floor(sy)),0,height-1);
        const int x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);
        const float fx=std::clamp(sx-x0,0.f,1.f),fy=std::clamp(sy-y0,0.f,1.f);
        for(int c=0;c<4;++c)
        {
            const float a=pixels[(size_t(y0)*width+x0)*4+c]*(1-fx)+pixels[(size_t(y0)*width+x1)*4+c]*fx;
            const float b=pixels[(size_t(y1)*width+x0)*4+c]*(1-fx)+pixels[(size_t(y1)*width+x1)*4+c]*fx;
            image->rgba[(size_t(row)*image->width+col)*4+c]=uint8_t(std::clamp(std::lround(a*(1-fy)+b*fy),0l,255l));
        }
    }
    return image;
}
} // namespace cd
