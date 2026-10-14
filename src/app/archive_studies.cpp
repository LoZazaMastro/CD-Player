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
    struct Command { int op=0; std::vector<float> data; Color color; float width=1; Rect clip; std::string text,align; };
    JSRuntime *runtime=nullptr;
    JSContext *context=nullptr;
    std::vector<Command> commands;
    std::string root,problem;
    unsigned selection=0;
    std::array<float,256> waveform{};
    int64_t deadline=0;
    ~Impl(){reset();}
    void reset(){if(context)JS_FreeContext(context);if(runtime)JS_FreeRuntime(runtime);context=nullptr;runtime=nullptr;commands.clear();}
    static double number(JSContext *ctx,JSValueConst value){double out=0;JS_ToFloat64(ctx,&out,value);return std::isfinite(out)?out:0;}
    static std::string string(JSContext *ctx,JSValueConst value){const char *p=JS_ToCString(ctx,value);std::string out=p?p:"";if(p)JS_FreeCString(ctx,p);return out;}
    static std::vector<float> array(JSContext *ctx,JSValueConst value){JSValue length=JS_GetPropertyStr(ctx,value,"length");const size_t n=std::clamp(size_t(number(ctx,length)),size_t(0),size_t(8192));JS_FreeValue(ctx,length);std::vector<float> out;out.reserve(n);for(size_t i=0;i<n;i++){JSValue v=JS_GetPropertyUint32(ctx,value,uint32_t(i));out.push_back(float(number(ctx,v)));JS_FreeValue(ctx,v);}return out;}
    static JSValue clear(JSContext *ctx,JSValueConst,int,JSValueConst*){auto *self=static_cast<Impl*>(JS_GetContextOpaque(ctx));self->commands.clear();return JS_UNDEFINED;}
    static JSValue paint(JSContext *ctx,JSValueConst,int argc,JSValueConst *args){
        auto *self=static_cast<Impl*>(JS_GetContextOpaque(ctx));if(argc!=6)return JS_ThrowTypeError(ctx,"paint ABI");
        if(self->commands.size()>=20000)return JS_ThrowRangeError(ctx,"study command limit");
        Command c;c.op=int(number(ctx,args[0]));c.color=Color::rgb(uint32_t(number(ctx,args[2])),float(number(ctx,args[3])));c.width=float(number(ctx,args[4]));
        auto clip=array(ctx,args[5]);if(clip.size()==4)c.clip={clip[0],clip[1],clip[2],clip[3]};
        if(c.op==3){JSValue text=JS_GetPropertyUint32(ctx,args[1],0),align=JS_GetPropertyUint32(ctx,args[1],4);c.text=string(ctx,text);c.align=string(ctx,align);JS_FreeValue(ctx,text);JS_FreeValue(ctx,align);for(unsigned i=1;i<=3;i++){JSValue v=JS_GetPropertyUint32(ctx,args[1],i);c.data.push_back(float(number(ctx,v)));JS_FreeValue(ctx,v);}}
        else c.data=array(ctx,args[1]);
        self->commands.push_back(std::move(c));return JS_UNDEFINED;
    }
    static int interrupt(JSRuntime*,void *opaque){auto *self=static_cast<Impl*>(opaque);return hui::sys::monotonic_us()>self->deadline;}
    bool evaluate(const std::string &source,const char *name){
        JSValue result=JS_Eval(context,source.data(),source.size(),name,JS_EVAL_TYPE_GLOBAL);
        if(JS_IsException(result)){JSValue error=JS_GetException(context);problem=string(context,error);JSValue trace=JS_GetPropertyStr(context,error,"stack");problem+=" "+string(context,trace);JS_FreeValue(context,trace);JS_FreeValue(context,error);JS_FreeValue(context,result);return false;}
        JS_FreeValue(context,result);return true;
    }
    bool select(unsigned selected){
        reset();problem.clear();selection=selected;runtime=JS_NewRuntime();if(!runtime){problem="JS allocation failed";return false;}
        JS_SetMemoryLimit(runtime,32u<<20);JS_SetMaxStackSize(runtime,512u<<10);JS_SetInterruptHandler(runtime,interrupt,this);deadline=hui::sys::monotonic_us()+2000000;
        context=JS_NewContext(runtime);if(!context){problem="JS context failed";return false;}JS_SetContextOpaque(context,this);
        JSValue global=JS_GetGlobalObject(context);JS_SetPropertyStr(context,global,"__paint",JS_NewCFunction(context,paint,"__paint",6));JS_SetPropertyStr(context,global,"__clear",JS_NewCFunction(context,clear,"__clear",0));JS_FreeValue(context,global);
        std::string bootstrap,program;
        if(!hui::save::read_file(root+"/runtime.js",&bootstrap) || !hui::save::read_file(root+"/"+std::string(archive_entries[selected-1].family)+".json",&program)){problem="Missing archive program";return false;}
        if(!evaluate(bootstrap,"archive-runtime.js"))return false;
        return evaluate("__mount("+program+","+std::to_string(archive_entries[selected-1].index)+");","archive-study.js");
    }
};
ArchiveStudies::ArchiveStudies():impl_(std::make_unique<Impl>()){}
ArchiveStudies::~ArchiveStudies()=default;
void ArchiveStudies::assets(const std::string &path){impl_->root=path+"/visualizers";}
void ArchiveStudies::waveform(const std::array<float,256> &value){impl_->waveform=value;}
const std::string &ArchiveStudies::error()const{return impl_->problem;}
bool ArchiveStudies::draw(hui::gfx::DrawList &out,unsigned selected,float time,float low,float mid,float high,float level,unsigned beat,float kick_age,float snare_age,const std::array<float,32> &bands,const hui::ui::Fonts &fonts){
    if(selected<1 || selected>archive_entries.size())return false;
    if(impl_->selection!=selected && !impl_->select(selected)){hui::sys::log("[CD visualizer] %u: %s",selected,impl_->problem.c_str());return false;}
    if(!impl_->problem.empty())return false;
    std::string samples="[";for(size_t i=0;i<bands.size();i++){if(i)samples+=",";samples+=std::to_string(bands[i]);}samples+="]";
    std::string wave="[";for(size_t i=0;i<impl_->waveform.size();i++){if(i)wave+=",";wave+=std::to_string(impl_->waveform[i]);}wave+="]";
    const std::string script="__tick("+std::to_string(time*1000)+",{active:true,source:'cd',mode:'play',low:"+std::to_string(low)+",mid:"+std::to_string(mid)+",high:"+std::to_string(high)+",level:"+std::to_string(level)+",amp:"+std::to_string(level)+",hit:"+std::to_string(std::exp(-kick_age*9))+",beat:"+std::to_string(beat)+",kickAge:"+std::to_string(kick_age)+",snareAge:"+std::to_string(snare_age)+",hatAge:"+std::to_string(snare_age)+",songTime:"+std::to_string(time)+",bandAt(b,age=0){return __past(b==='amp'?'level':b,age,this);},eventAt(k,age=0){let s=__pastSignal(age,this);return {n:s.beat,age:(s[k+'Age']??30)+age};},spectrumAt(x){return "+samples+"[Math.min(31,Math.max(0,Math.floor(x*31)))];},levelAt(age=0){return __past('level',age,this);},hitAt(age=0){return __past('hit',age,this);},sampleAt(age=0){return "+wave+"[Math.max(0,Math.min(255,255-Math.floor(age*48000/4)))];}});";
    impl_->deadline=hui::sys::monotonic_us()+100000;
    if(!impl_->evaluate(script,"archive-frame.js")){hui::sys::log("[CD visualizer] %u: %s",selected,impl_->problem.c_str());return false;}
    constexpr float scale=1320.f/412.f;
    out.push_transform(scale,0,0,300,440);
    for(const auto &c:impl_->commands){
        out.push_clip(c.clip);
        const auto &v=c.data;
        if(c.op==0 && v.size()==4)out.rounded_rect({v[0],v[1],v[2],v[3]},0,c.color);
        else if(c.op==1 && v.size()==4)out.line(v[0],v[1],v[2],v[3],c.width,c.color);
        else if(c.op==2 && v.size()>=6)out.polygon(v.data(),int(v.size()/2),c.color);
        else if(c.op==7 && v.size()>=12){std::vector<float> xy;std::vector<Color> colors;for(size_t i=0;i+3<v.size();i+=4){xy.push_back(v[i]);xy.push_back(v[i+1]);colors.push_back(Color::rgb(uint32_t(v[i+2]),v[i+3]*c.color.a));}out.polygon_colored(xy.data(),int(colors.size()),colors.data());}
        else if(c.op==3 && v.size()==3)hui::ui::text(out,fonts.display,c.text,v[0],v[1],v[2],c.color,c.align=="center"?hui::gfx::Align::center:c.align=="right"?hui::gfx::Align::right:hui::gfx::Align::left);
        out.pop_clip();
    }
    out.pop_transform();
    return !impl_->commands.empty();
}
}
