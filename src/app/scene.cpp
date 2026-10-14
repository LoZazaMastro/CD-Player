// CD Player - Perspective optical disc with reflective room lighting and diffraction.
// Copyright (C) 2026 Andrea
// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/scene.hpp"
#include "gfx/gl_program.hpp"
#include "third_party/stb/stb_image.h"

namespace cd
{
namespace
{
const char *vertex = R"(
out vec2 uv;
void main(){ vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2)); uv=p; gl_Position=vec4(p*2.-1.,0.,1.); }
)";
const char *fragment = R"(
in vec2 uv;out vec4 color;
layout(location=0) uniform float spin;
layout(location=1) uniform vec2 size;
layout(binding=0) uniform sampler2D engraving;
mat2 rot(float a){float c=cos(a),s=sin(a);return mat2(c,-s,s,c);}
// A small spindle precession changes the view of the fixed room lights.
// The two transforms remain exact inverses, including the precession.
vec3 localSpace(vec3 p){p.xz=rot(.23+.035*cos(spin))*p.xz;p.yz=rot(-.13+.028*sin(spin))*p.yz;p.xy=rot(spin)*p.xy;return p;}
vec3 worldSpace(vec3 p){p.xy=rot(-spin)*p.xy;p.yz=rot(.13-.028*sin(spin))*p.yz;p.xz=rot(-.23-.035*cos(spin))*p.xz;return p;}
float panel(vec3 ray,vec3 axis,vec2 extent){
    vec3 tangent=normalize(cross(axis,vec3(0,1,0))),up=cross(axis,tangent);
    float facing=max(dot(ray,axis),.01);
    vec2 p=vec2(dot(ray,tangent),dot(ray,up))/facing/extent;
    return exp(-pow(abs(p.x),6.)-pow(abs(p.y),6.))*smoothstep(.1,.5,facing);
}
vec3 room(vec3 ray){
    float a=panel(ray,normalize(vec3(-.6,.35,1.)),vec2(.38,.72));
    float b=panel(ray,normalize(vec3(.8,-.2,.6)),vec2(.16,.85));
    float c=panel(ray,normalize(vec3(-.1,.9,.45)),vec2(.8,.10));
    return vec3(.22,.23,.25)+a*vec3(.72,.74,.77)+b*vec3(.52,.49,.44)+c*.65;
}
void main(){
    vec2 screen=(uv*2.-1.)*vec2(size.x/size.y,1.);
    vec3 origin=localSpace(vec3(0,0,2.6));
    vec3 direction=localSpace(normalize(vec3(screen,-2.25)));
    float t=(.012-origin.z)/direction.z;
    vec3 p=origin+direction*t,normal=vec3(0,0,1);
    float radius=length(p.xy);bool rim=false;
    if(radius>.86 || radius<.112){
        float surface=radius>.86?.86:.112;
        float a=dot(direction.xy,direction.xy),b=dot(origin.xy,direction.xy);
        float c=dot(origin.xy,origin.xy)-surface*surface;
        float d=b*b-a*c;
        if(a<.00001 || d<0.){color=vec4(0);return;}
        float signRoot=surface>.5?-1.:1.;
        t=(-b+signRoot*sqrt(d))/a;p=origin+direction*t;
        if(t<0. || abs(p.z)>.012){color=vec4(0);return;}
        radius=length(p.xy);normal=vec3(normalize(p.xy)*(surface>.5?1.:-1.),0);rim=true;
    }
    // Subtle broad polishing variations travel with the surface. Without
    // these, an ideal circular grating looks stationary despite rotating.
    if(!rim && radius>.315){
        float a=atan(p.y,p.x);
        float polish=.016*cos(2.*a+.45)+.008*sin(3.*a-1.2);
        normal=normalize(vec3(polish*normalize(p.xy),1.));
    }
    vec3 reflection=worldSpace(reflect(direction,normal));
    vec3 silver=room(reflection);
    vec3 material=silver*.62+vec3(.17);
    float a=atan(p.y,p.x),grain=sin(a*131.+radius*420.);
    material*=1.+grain*.012;
    if(radius>.315 && !rim){
        vec3 light=localSpace(normalize(vec3(-.45,.8,1.)));
        float grating=dot(normalize(p.xy),light.xy-direction.xy);
        vec3 diffraction=.5+.5*cos(6.2831853*(grating*.82+vec3(0.,.31,.64)));
        float rainbow=.48*pow(max(dot(-direction,normal),0.),2.);
        rainbow*=.86+.14*cos(2.*a+.45);
        material=mix(material,diffraction*.75+silver*.38,rainbow);
        float groove=sin(radius*2600.)*(1.-smoothstep(.7,1.7,fwidth(radius)*2600.));
        material*=.99+.012*groove;
    }
    if(radius<.315 && !rim){
        material=mix(vec3(.12),silver,.57);
        material*=.91+.05*sin(radius*290.);
        if(radius<.155)material=silver*.83+vec3(.11);
    }
    if(rim || radius>.851 || abs(radius-.315)<.004)material=silver*.90+vec3(.17);
    if(!rim && p.x>.33 && p.x<.80 && abs(p.y)<.054){
        vec2 label=(p.xy-vec2(.565,0.))/vec2(.47,.108)+.5;
        label.y=1.-label.y;
        float mark=texture(engraving,label).r;
        vec2 step=1./vec2(textureSize(engraving,0));
        float bevel=texture(engraving,label+step).r-texture(engraving,label-step).r;
        material=mix(material,material*.22+silver*.045,mark*.92);
        material+=bevel*.12;
    }
    color=vec4(clamp(material,0.,1.),1.);
}
)";
} // namespace
Scene::~Scene()
{
    if (program_)
        glDeleteProgram(program_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
    if (engraving_)
        glDeleteTextures(1, &engraving_);
}
bool Scene::init(const std::string &assets)
{
    program_ = hui::gfx::build_program("CD spatial scene", vertex, fragment);
    if (!program_)
        return false;
    glGenVertexArrays(1, &vao_);
    int width = 0, height = 0, channels = 0;
    auto *pixels =
        stbi_load((assets + "/disc/engraving.png").c_str(), &width, &height, &channels, 1);
    if (!pixels)
        return false;
    glGenTextures(1, &engraving_);
    glBindTexture(GL_TEXTURE_2D, engraving_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    return true;
}
GLuint Scene::render(float spin, int width, int height)
{
    if (canvas_.width() != width || canvas_.height() != height)
        if (!canvas_.create(width, height, 1))
            return 0;
    canvas_.bind();
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, engraving_);
    glUniform1f(0, spin);
    glUniform2f(1, float(width), float(height));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    canvas_.resolve();
    canvas_.unbind();
    return canvas_.texture();
}
} // namespace cd
