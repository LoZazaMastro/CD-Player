// CD Player - Perspective jewel case with independent front, back and spine.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/cover_scene.hpp"
#include "gfx/gl_program.hpp"

namespace cd
{
namespace
{
const char *vertex = R"(
out vec2 uv;
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));uv=p;gl_Position=vec4(p*2.-1.,0,1);}
)";
const char *fragment = R"(
in vec2 uv;out vec4 color;
layout(location=0) uniform float angle;
layout(location=1) uniform vec3 present;
layout(location=2) uniform vec2 artFade;
layout(binding=0) uniform sampler2D frontImage;
layout(binding=1) uniform sampler2D backImage;
layout(binding=2) uniform sampler2D spineImage;
vec3 local(vec3 p){float c=cos(angle),s=sin(angle);return vec3(c*p.x-s*p.z,p.y,s*p.x+c*p.z);}
vec3 world(vec3 p){float c=cos(angle),s=sin(angle);return vec3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
void main(){
    float frame=.94*(1.+.35*abs(sin(angle)));
    vec3 ro=local(vec3(0,0,3.4)),rd=local(normalize(vec3((uv*2.-1.)*frame,-3.4)));
    vec3 bounds=vec3(.9,.9,.045);
    vec3 safe=vec3(abs(rd.x)<.000001?.000001:rd.x,abs(rd.y)<.000001?.000001:rd.y,abs(rd.z)<.000001?.000001:rd.z);
    vec3 t0=(-bounds-ro)/safe,t1=(bounds-ro)/safe;
    vec3 nearT=min(t0,t1),farT=max(t0,t1);
    float entry=max(max(nearT.x,nearT.y),nearT.z),exitT=min(min(farT.x,farT.y),farT.z);
    if(entry>exitT || exitT<0.){color=vec4(0);return;}
    vec3 p=ro+rd*entry,n;
    vec3 base=vec3(.065);
    if(nearT.z>=nearT.x && nearT.z>=nearT.y){
        n=vec3(0,0,sign(p.z));
        vec2 tex=vec2((p.z>0.?p.x:-p.x)/1.8+.5,.5-p.y/1.8);
        if(p.z>0. && present.x>.5)base=texture(frontImage,tex).rgb;
        if(p.z<0. && present.y>.5)base=mix(base,texture(backImage,tex).rgb,artFade.x);
        float edge=1.-smoothstep(.008,.025,min(.9-abs(p.x),.9-abs(p.y)));
        base=mix(base,vec3(.16),edge*.55);
    }else if(nearT.x>=nearT.y){
        n=vec3(sign(p.x),0,0);
        if(present.z>.5)base=mix(base,texture(spineImage,vec2(.5-p.z/.09,.5-p.y/1.8)).rgb,artFade.y);
    }else{n=vec3(0,sign(p.y),0);base=vec3(.12);}
    vec3 normal=world(n),view=world(-rd),reflection=reflect(-view,normal);
    float fresnel=.025+.12*pow(1.-max(dot(normal,view),0.),4.);
    float softbox=exp(-pow(abs((reflection.x+.42)/.32),6.)-pow(abs((reflection.y-.25)/.85),6.));
    float strip=exp(-pow(abs((reflection.x-.55)/.075),2.));
    // The room light stays fixed while the case and its printed surfaces rotate.
    vec3 gloss=vec3(.3)*fresnel+vec3(softbox*.16+strip*.10);
    color=vec4(clamp(base*(.94+.06*max(normal.z,0.))+gloss,0.,1.),1.);
}
)";
}
CoverScene::~CoverScene()
{
    if(program_)glDeleteProgram(program_);
    if(vao_)glDeleteVertexArrays(1,&vao_);
}
bool CoverScene::init()
{
    program_=hui::gfx::build_program("CD jewel case",vertex,fragment);
    if(!program_)return false;
    glGenVertexArrays(1,&vao_);
    return true;
}
GLuint CoverScene::render(float angle,GLuint front,GLuint back,GLuint spine,float back_fade,float spine_fade,int size)
{
    if(canvas_.width()==size && angle==last_angle_ && front==last_front_ && back==last_back_ &&
       spine==last_spine_ && back_fade==last_back_fade_ && spine_fade==last_spine_fade_)
        return canvas_.texture();
    if(canvas_.width()!=size && !canvas_.create(size,size,1))return 0;
    canvas_.bind();
    glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
    glUseProgram(program_);glBindVertexArray(vao_);
    for(unsigned i=0;i<3;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,i==0?front:i==1?back:spine);}
    glUniform1f(0,angle);glUniform3f(1,front?1.f:0.f,back?1.f:0.f,spine?1.f:0.f);
    glUniform2f(2,back_fade,spine_fade);
    glDrawArrays(GL_TRIANGLES,0,3);canvas_.resolve();canvas_.unbind();
    last_angle_=angle;last_front_=front;last_back_=back;last_spine_=spine;
    last_back_fade_=back_fade;last_spine_fade_=spine_fade;
    return canvas_.texture();
}
}
