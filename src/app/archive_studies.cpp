// CD Player - Original iwrzwr drawing programs rendered through Prospero UI.
// Copyright (C) 2026 Andrea; drawing programs (C) 2026 Kagan Yaldizkaya, MIT.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/archive_studies.hpp"
#include "app/archive_catalog.hpp"
#include "core/save_file.hpp"
#include "platform/ps5/system.hpp"
#include "ui/widgets.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"
extern "C" {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "third_party/quickjs/quickjs.h"
#pragma clang diagnostic pop
}
namespace cd {
using hui::gfx::Color;
using hui::gfx::Rect;
struct ArchiveStudies::Impl {
    struct Command { int op=0; std::vector<float> data; Color color; float width=1; Rect clip; std::string text,align,baseline; };
    JSRuntime *runtime=nullptr;
    JSContext *context=nullptr;
    std::vector<Command> commands;
    size_t command_count=0;
    uint64_t redraw_count=0;
    unsigned missing=0;
    std::array<float,288> audio_samples{};
    JSValue frame_function=JS_UNDEFINED;
    std::string root,problem;
    unsigned selection=0;
    std::array<float,256> waveform{};
    int64_t deadline=0;
    ~Impl(){reset();}
    void reset(){if(context){JS_FreeValue(context,frame_function);JS_FreeContext(context);}if(runtime)JS_FreeRuntime(runtime);frame_function=JS_UNDEFINED;context=nullptr;runtime=nullptr;commands.clear();command_count=0;redraw_count=0;missing=0;}
    static double number(JSContext *ctx,JSValueConst value){double out=0;JS_ToFloat64(ctx,&out,value);return std::isfinite(out)?out:0;}
    static std::string string(JSContext *ctx,JSValueConst value){const char *p=JS_ToCString(ctx,value);std::string out=p?p:"";if(p)JS_FreeCString(ctx,p);return out;}
    static void array(JSContext *ctx,JSValueConst value,std::vector<float> &out){JSValue length=JS_GetPropertyStr(ctx,value,"length");const size_t n=size_t(std::clamp(number(ctx,length),0.,8192.));JS_FreeValue(ctx,length);out.clear();out.reserve(n);for(size_t i=0;i<n;i++){JSValue v=JS_GetPropertyUint32(ctx,value,uint32_t(i));out.push_back(float(number(ctx,v)));JS_FreeValue(ctx,v);}}
    static JSValue clear(JSContext *ctx,JSValueConst,int,JSValueConst*){auto *self=static_cast<Impl*>(JS_GetContextOpaque(ctx));self->command_count=0;++self->redraw_count;return JS_UNDEFINED;}
    static JSValue paint(JSContext *ctx,JSValueConst,int argc,JSValueConst *args){
        auto *self=static_cast<Impl*>(JS_GetContextOpaque(ctx));if(argc!=6)return JS_ThrowTypeError(ctx,"paint ABI");
        if(self->command_count>=20000)return JS_ThrowRangeError(ctx,"study command limit");
        if(self->command_count==self->commands.size())self->commands.emplace_back();
        Command &c=self->commands[self->command_count++];c.data.clear();c.text.clear();c.align.clear();c.baseline.clear();
        c.op=int(number(ctx,args[0]));c.color=Color::rgb(uint32_t(number(ctx,args[2])),float(number(ctx,args[3])));c.width=float(number(ctx,args[4]));
        float clip[4];for(unsigned i=0;i<4;i++){JSValue v=JS_GetPropertyUint32(ctx,args[5],i);clip[i]=float(number(ctx,v));JS_FreeValue(ctx,v);}c.clip={clip[0],clip[1],clip[2],clip[3]};
        if(c.op==3){JSValue text=JS_GetPropertyUint32(ctx,args[1],0),align=JS_GetPropertyUint32(ctx,args[1],4),baseline=JS_GetPropertyUint32(ctx,args[1],5);c.text=string(ctx,text);c.align=string(ctx,align);c.baseline=string(ctx,baseline);JS_FreeValue(ctx,text);JS_FreeValue(ctx,align);JS_FreeValue(ctx,baseline);for(unsigned i=1;i<=3;i++){JSValue v=JS_GetPropertyUint32(ctx,args[1],i);c.data.push_back(float(number(ctx,v)));JS_FreeValue(ctx,v);}}
        else array(ctx,args[1],c.data);
        return JS_UNDEFINED;
    }
    static int interrupt(JSRuntime*,void *opaque){auto *self=static_cast<Impl*>(opaque);return hui::sys::monotonic_us()>self->deadline;}
    bool result_ok(JSValue result){
        if(JS_IsException(result)){JSValue error=JS_GetException(context);problem=string(context,error);JSValue trace=JS_GetPropertyStr(context,error,"stack");problem+=" "+string(context,trace);JS_FreeValue(context,trace);JS_FreeValue(context,error);JS_FreeValue(context,result);return false;}
        JS_FreeValue(context,result);return true;
    }
    bool evaluate(const std::string &source,const char *name){return result_ok(JS_Eval(context,source.data(),source.size(),name,JS_EVAL_TYPE_GLOBAL));}
    static void release_samples(JSRuntime*,void*,void*){} // the buffer belongs to Impl
    bool select(unsigned selected){
        reset();problem.clear();selection=selected;runtime=JS_NewRuntime();if(!runtime){problem="JS allocation failed";return false;}
        JS_SetMemoryLimit(runtime,32u<<20);JS_SetMaxStackSize(runtime,512u<<10);JS_SetInterruptHandler(runtime,interrupt,this);deadline=hui::sys::monotonic_us()+2000000;
        context=JS_NewContext(runtime);if(!context){problem="JS context failed";return false;}JS_SetContextOpaque(context,this);
        JSValue global=JS_GetGlobalObject(context);JS_SetPropertyStr(context,global,"__paint",JS_NewCFunction(context,paint,"__paint",6));JS_SetPropertyStr(context,global,"__clear",JS_NewCFunction(context,clear,"__clear",0));
        JS_SetPropertyStr(context,global,"__audioSamples",JS_NewArrayBuffer(context,reinterpret_cast<uint8_t*>(audio_samples.data()),sizeof(audio_samples),release_samples,nullptr,false));JS_FreeValue(context,global);
        std::string bootstrap,program;
        if(!hui::save::read_file(root+"/runtime.js",&bootstrap) || !hui::save::read_file(root+"/"+std::string(archive_entries[selected-1].family)+".json",&program)){problem="Missing archive program";return false;}
        if(!evaluate(bootstrap,"archive-runtime.js"))return false;
        if(!evaluate("__mount("+program+","+std::to_string(archive_entries[selected-1].index)+");","archive-study.js"))return false;
        global=JS_GetGlobalObject(context);frame_function=JS_GetPropertyStr(context,global,"__nativeFrame");JS_FreeValue(context,global);
        return JS_IsFunction(context,frame_function);
    }
};
ArchiveStudies::ArchiveStudies():impl_(std::make_unique<Impl>()){}
ArchiveStudies::~ArchiveStudies()=default;
void ArchiveStudies::assets(const std::string &path){impl_->root=path+"/visualizers";}
void ArchiveStudies::waveform(const std::array<float,256> &value){impl_->waveform=value;}
const std::string &ArchiveStudies::error()const{return impl_->problem;}
uint64_t ArchiveStudies::redraws()const{return impl_->redraw_count;}
unsigned ArchiveStudies::missing_glyphs()const{return impl_->missing;}
bool ArchiveStudies::draw(hui::gfx::DrawList &out,unsigned selected,float time,float low,float mid,float high,float level,unsigned beat,float kick_age,float snare_age,const std::array<float,32> &bands,const hui::ui::Fonts &fonts){
    if(selected<1 || selected>archive_entries.size())return false;
    if(impl_->selection!=selected && !impl_->select(selected)){hui::sys::log("[CD visualizer] %u: %s",selected,impl_->problem.c_str());return false;}
    if(!impl_->problem.empty())return false;
    std::copy(bands.begin(),bands.end(),impl_->audio_samples.begin());
    std::copy(impl_->waveform.begin(),impl_->waveform.end(),impl_->audio_samples.begin()+32);
    const double values[]={double(time)*1000,low,mid,high,level,double(beat),kick_age,snare_age};
    JSValue args[8];for(unsigned i=0;i<8;i++)args[i]=JS_NewFloat64(impl_->context,values[i]);
    impl_->deadline=hui::sys::monotonic_us()+100000;
    const bool okay=impl_->result_ok(JS_Call(impl_->context,impl_->frame_function,JS_UNDEFINED,8,args));
    for(auto &arg:args)JS_FreeValue(impl_->context,arg);
    if(!okay){hui::sys::log("[CD visualizer] %u: %s",selected,impl_->problem.c_str());return false;}
    constexpr float scale=1320.f/412.f;
    out.push_transform(scale,0,0,300,440);
    impl_->missing=0;
    for(size_t index=0;index<impl_->command_count;++index){
        const auto &c=impl_->commands[index];
        out.push_clip(c.clip);
        const auto &v=c.data;
        if(c.op==0 && v.size()==4)out.rounded_rect({v[0],v[1],v[2],v[3]},0,c.color);
        else if(c.op==1 && v.size()==4)out.line(v[0],v[1],v[2],v[3],c.width,c.color);
        else if(c.op==2 && v.size()>=6)out.polygon(v.data(),int(v.size()/2),c.color);
        else if(c.op==7 && v.size()>=12){std::vector<float> xy;std::vector<Color> colors;for(size_t i=0;i+3<v.size();i+=4){xy.push_back(v[i]);xy.push_back(v[i+1]);colors.push_back(Color::rgb(uint32_t(v[i+2]),v[i+3]*c.color.a));}out.polygon_colored(xy.data(),int(colors.size()),colors.data());}
        else if(c.op==3 && v.size()==3){
            const auto &face=fonts.mono.font?fonts.mono:fonts.display;
            for(size_t at=0;at<c.text.size();){const auto cp=hui::gfx::next_codepoint(c.text,&at);if(!face.font->has_glyph(cp))++impl_->missing;}
            float y=v[1];
            if(c.baseline=="middle")y+=(face.font->ascent(v[2])-face.font->descent(v[2]))*.5f;
            else if(c.baseline=="top" || c.baseline=="hanging")y+=face.font->ascent(v[2]);
            else if(c.baseline=="bottom")y-=face.font->descent(v[2]);
            hui::ui::text(out,face,c.text,v[0],y,v[2],c.color,c.align=="center"?hui::gfx::Align::center:c.align=="right"?hui::gfx::Align::right:hui::gfx::Align::left);
        }
        out.pop_clip();
    }
    out.pop_transform();
    return impl_->command_count!=0;
}
}
