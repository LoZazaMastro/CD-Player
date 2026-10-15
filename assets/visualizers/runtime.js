// Native canvas adapter for iwrzwr's original drawing programs.
// Copyright (C) 2026 Andrea. SPDX-License-Identifier: GPL-3.0-or-later
'use strict';
globalThis.window=globalThis; globalThis.devicePixelRatio=1;
globalThis.__now=0; globalThis.__raf=new Map();globalThis.__next=0;
globalThis.__resize=[];globalThis.__selected=null;
globalThis.performance={now:()=>__now};
globalThis.requestAnimationFrame=fn=>{let id=++__next;__raf.set(id,fn);return id;};
globalThis.cancelAnimationFrame=id=>__raf.delete(id);
globalThis.matchMedia=()=>({matches:false,addEventListener(){},removeEventListener(){}});
globalThis.addEventListener=()=>{};
globalThis.lucide={createIcons(){}};
globalThis.CSS={escape:s=>s};
globalThis.getComputedStyle=()=>({getPropertyValue:()=> '#fff'});
function matches(node,selector){
  selector=selector.trim();if(!selector)return false;
  let split=selector.lastIndexOf(' ');
  if(split>=0){let first=selector.slice(0,split),last=selector.slice(split+1);if(!matches(node,last))return false;let parent=node.parentElement;while(parent){if(matches(parent,first))return true;parent=parent.parentElement;}return false;}
  let tag=selector.match(/^[\w-]+/);if(tag && node.tagName.toLowerCase()!==tag[0].toLowerCase())return false;
  for(let id of selector.matchAll(/#([\w-]+)/g))if(node.id!==id[1])return false;
  for(let cls of selector.matchAll(/\.([\w-]+)/g))if(!(node.attrs.class||'').split(/\s+/).includes(cls[1]))return false;
  for(let attr of selector.matchAll(/\[([^=\]]+)(?:=["']?([^\]"']+)["']?)?\]/g)){if(!(attr[1] in node.attrs))return false;if(attr[2]!==undefined && node.attrs[attr[1]]!==attr[2])return false;}
  return true;
}
class Element {
  constructor(data){this.tagName=data.tag.toUpperCase();this.attrs={...data.attrs};this.id=this.attrs.id||'';this.dataset={};for(let key in this.attrs)if(key.startsWith('data-'))this.dataset[key.slice(5).replace(/-([a-z])/g,(_,c)=>c.toUpperCase())]=this.attrs[key];this.children=[];this.listeners={};this.style={setProperty(){}};this.hidden=false;this.isConnected=true;this.width=Number(this.attrs.width)||412;this.height=Number(this.attrs.height)||44;this.clientWidth=412;this.clientHeight=44;this.classList={add(){},remove(){},toggle(){}};}
  querySelectorAll(selector){let found=[];const selectors=selector.split(',');let scan=n=>{for(let c of n.children){if(selectors.some(s=>matches(c,s)))found.push(c);scan(c);}};scan(this);return found;}
  querySelector(selector){return this.querySelectorAll(selector)[0]||null;}
  getAttribute(key){return this.attrs[key]??null;} setAttribute(key,value){this.attrs[key]=String(value);}
  addEventListener(name,fn){(this.listeners[name]??=[]).push(fn);} removeEventListener(){}
  click(){for(let fn of this.listeners.click||[])fn({target:this,currentTarget:this});}
  append(...nodes){for(let node of nodes){node.parentElement=this;node.parentNode=this;this.children.push(node);}}
  appendChild(node){this.append(node);return node;}
  closest(selector){let n=this;while(n){if(matches(n,selector))return n;n=n.parentElement;}return null;}
  contains(child){while(child){if(child===this)return true;child=child.parentElement;}return false;}
  getBoundingClientRect(){return {width:412,height:44,x:0,y:0,left:0,top:0,right:412,bottom:44};}
  getContext(){return this.context??=new Canvas(this);}
}
globalThis.ResizeObserver=class{constructor(fn){this.fn=fn;}observe(target){__resize.push(()=>this.fn([{target,contentRect:target.getBoundingClientRect()}]));}disconnect(){}};
globalThis.IntersectionObserver=class{constructor(fn){this.fn=fn;}observe(target){__resize.push(()=>this.fn([{target,isIntersecting:target.contains(__selected),time:__now}]));}disconnect(){}};
let colorCache=new Map();
function color(value){
  if(typeof value==='object')return color(value.stops?.[0]?.[1]||'#fff');
  if(colorCache.has(value))return colorCache.get(value);
  let text=String(value||'#fff'),r=255,g=255,b=255,a=1;
  if(text[0]==='#'){let h=text.slice(1);if(h.length===3||h.length===4)h=[...h].map(x=>x+x).join('');r=parseInt(h.slice(0,2),16);g=parseInt(h.slice(2,4),16);b=parseInt(h.slice(4,6),16);if(h.length===8)a=parseInt(h.slice(6,8),16)/255;}
  else if(text.startsWith('rgb')){let c=text.match(/[\d.]+/g).map(Number);[r,g,b]=c;if(c.length>3)a=c[3];}
  else if(text==='black')r=g=b=0;
  let result=[(r<<16)|(g<<8)|b,a];colorCache.set(value,result);return result;
}
class Canvas {
  constructor(canvas){this.canvas=canvas;this.globalAlpha=1;this.fillStyle='#fff';this.strokeStyle='#fff';this.lineWidth=1;this.lineCap='butt';this.font='10px sans-serif';this.textAlign='left';this.textBaseline='alphabetic';this.matrix=[1,0,0,1,0,0];this.clipping=[0,0,412,44];this.stack=[];this.path=[];this.paths=[];}
  point(x,y){let m=this.matrix;return [m[0]*x+m[2]*y+m[4],m[1]*x+m[3]*y+m[5]];}
  emit(op,data,style=this.fillStyle){if(this.canvas!==__selected)return;let [rgb,alpha]=color(style);if(alpha*this.globalAlpha<=0)return;__paint(op,data,rgb,alpha*this.globalAlpha,this.lineWidth,this.clipping);}
  save(){this.stack.push({matrix:[...this.matrix],clipping:[...this.clipping],globalAlpha:this.globalAlpha,fillStyle:this.fillStyle,strokeStyle:this.strokeStyle,lineWidth:this.lineWidth,font:this.font,textAlign:this.textAlign,textBaseline:this.textBaseline});}
  restore(){if(this.stack.length)Object.assign(this,this.stack.pop());}
  setTransform(a,b,c,d,e,f){this.matrix=[a,b,c,d,e,f];}
  resetTransform(){this.setTransform(1,0,0,1,0,0);}
  transform(a,b,c,d,e,f){let m=this.matrix;this.matrix=[m[0]*a+m[2]*b,m[1]*a+m[3]*b,m[0]*c+m[2]*d,m[1]*c+m[3]*d,m[0]*e+m[2]*f+m[4],m[1]*e+m[3]*f+m[5]];}
  translate(x,y){this.transform(1,0,0,1,x,y);}scale(x,y){this.transform(x,0,0,y,0,0);}rotate(a){this.transform(Math.cos(a),Math.sin(a),-Math.sin(a),Math.cos(a),0,0);}
  clearRect(){if(this.canvas===__selected)__clear();}
  beginPath(){this.path=[];this.paths=[];}
  moveTo(x,y){if(this.path.length)this.paths.push(this.path);this.path=[this.point(x,y)];}
  lineTo(x,y){this.path.push(this.point(x,y));}
  closePath(){if(this.path.length)this.path.push(this.path[0]);}
  rect(x,y,w,h){this.moveTo(x,y);this.lineTo(x+w,y);this.lineTo(x+w,y+h);this.lineTo(x,y+h);this.closePath();}
  clip(){let points=[...this.paths,this.path].flat();if(!points.length)return;let xs=points.map(p=>p[0]),ys=points.map(p=>p[1]);let a=Math.max(this.clipping[0],Math.min(...xs)),b=Math.max(this.clipping[1],Math.min(...ys)),c=Math.min(this.clipping[0]+this.clipping[2],Math.max(...xs)),d=Math.min(this.clipping[1]+this.clipping[3],Math.max(...ys));this.clipping=[a,b,Math.max(0,c-a),Math.max(0,d-b)];}
  stroke(){for(let path of [...this.paths,this.path])for(let i=1;i<path.length;i++)this.emit(1,[...path[i-1],...path[i]],this.strokeStyle);}
  fill(){for(let path of [...this.paths,this.path])if(path.length>=3){if(typeof this.fillStyle==='object'){
    let g=this.fillStyle,dx=g.end[0]-g.start[0],dy=g.end[1]-g.start[1],den=dx*dx+dy*dy||1;
    let values=[];for(let p of path){let t=Math.max(0,Math.min(1,((p[0]-g.start[0])*dx+(p[1]-g.start[1])*dy)/den));let a=g.stops[0],b=g.stops.at(-1);for(let i=1;i<g.stops.length;i++)if(t<=g.stops[i][0]){a=g.stops[i-1];b=g.stops[i];break;}let [ca,aa]=color(a[1]),[cb,ab]=color(b[1]),k=Math.max(0,Math.min(1,(t-a[0])/(b[0]-a[0]||1)));let channels=[16,8,0].map(s=>Math.round(((ca>>s)&255)*(1-k)+((cb>>s)&255)*k));values.push(...p,(channels[0]<<16)|(channels[1]<<8)|channels[2],aa*(1-k)+ab*k);}this.emit(7,values,'#fff');
  }else this.emit(2,path.flat());}}
  fillRect(x,y,w,h){let a=this.point(x,y),b=this.point(x+w,y+h);this.emit(0,[...a,b[0]-a[0],b[1]-a[1]]);}
  strokeRect(x,y,w,h){this.beginPath();this.rect(x,y,w,h);this.stroke();}
  arc(x,y,r,start,end,ccw=false){let sweep=end-start;if(!ccw){while(sweep<0)sweep+=Math.PI*2;sweep=Math.min(sweep,Math.PI*2);}else{while(sweep>0)sweep-=Math.PI*2;sweep=Math.max(sweep,-Math.PI*2);}let n=Math.max(3,Math.ceil(Math.abs(sweep)*Math.max(r,8)/2));n=Math.min(n,128);for(let i=0;i<=n;i++){let a=start+sweep*i/n;this.lineTo(x+Math.cos(a)*r,y+Math.sin(a)*r);}}
  ellipse(x,y,rx,ry,rotation,start,end,ccw){this.save();this.translate(x,y);this.rotate(rotation);this.scale(rx,ry);this.arc(0,0,1,start,end,ccw);this.restore();}
  bezierCurveTo(x1,y1,x2,y2,x3,y3){let p=this.path.at(-1)||[0,0],a=this.point(x1,y1),b=this.point(x2,y2),c=this.point(x3,y3);for(let i=1;i<=20;i++){let t=i/20,u=1-t;this.path.push([u*u*u*p[0]+3*u*u*t*a[0]+3*u*t*t*b[0]+t*t*t*c[0],u*u*u*p[1]+3*u*u*t*a[1]+3*u*t*t*b[1]+t*t*t*c[1]]);}}
  quadraticCurveTo(x1,y1,x2,y2){let p=this.path.at(-1)||[0,0],a=this.point(x1,y1),b=this.point(x2,y2);for(let i=1;i<=16;i++){let t=i/16,u=1-t;this.path.push([u*u*p[0]+2*u*t*a[0]+t*t*b[0],u*u*p[1]+2*u*t*a[1]+t*t*b[1]]);}}
  setLineDash(){}
  createLinearGradient(x1,y1,x2,y2){return {start:this.point(x1,y1),end:this.point(x2,y2),stops:[],addColorStop(p,c){this.stops.push([p,c]);this.stops.sort((a,b)=>a[0]-b[0]);}};}
  fillText(text,x,y){this.emit(3,[String(text),...this.point(x,y),parseFloat(this.font)||10,this.textAlign,this.textBaseline]);}
  measureText(text){return {width:String(text).length*(parseFloat(this.font)||10)*.6};}
}
globalThis.__mount=function(program,index){
  let nodes=program.nodes.map(n=>new Element(n));for(let i=1;i<nodes.length;i++){let parent=nodes[program.nodes[i].parent];nodes[i].parentElement=parent;nodes[i].parentNode=parent;parent.children.push(nodes[i]);}
  let body=nodes[0];globalThis.document={body,head:new Element({tag:'head',attrs:{}}),hidden:false,querySelector:body.querySelector.bind(body),querySelectorAll:body.querySelectorAll.bind(body),getElementById:id=>nodes.find(n=>n.id===id)||null,addEventListener(){},removeEventListener(){},createElement:tag=>new Element({tag,attrs:{}})};
  let buttons=document.querySelectorAll('button[data-style]'),canvases=document.querySelectorAll('canvas');
  __selected=canvases[buttons.length?0:index]||canvases[0];
  // The gallery throttles its embedded previews. Native playback follows the
  // display's requestAnimationFrame instead, without changing the drawing code.
  for(let source of program.scripts)(0,eval)(source.replace(/>=\s*1000\s*\/\s*(?:30|60)(?:\s*-\s*1)?/g,'>0'));
  canvases=document.querySelectorAll('canvas');__selected=canvases[buttons.length?0:index]||canvases[0];
  if(buttons.length)buttons[index]?.click();
  for(let resize of __resize)resize();__resize=[];
};
globalThis.__signalHistory=[];
globalThis.__pastSignal=function(age,fallback){let cutoff=__now-Math.max(0,age)*1000;for(let i=__signalHistory.length-1;i>=0;i--)if(__signalHistory[i].t<=cutoff)return __signalHistory[i].s;return fallback;};
globalThis.__past=function(band,age,fallback){return __pastSignal(age,fallback)[band]||0;};
globalThis.__tick=function(ms,signal){
  globalThis.__now=ms;globalThis.iwrSignal=signal;
  __signalHistory.push({t:ms,s:signal});if(__signalHistory.length>480)__signalHistory.shift();
  let callbacks=[...__raf.values()];__raf.clear();for(let fn of callbacks)fn(ms);
};

// Shared samples and a compiled entry point: C++ updates the buffer and calls
// this function, rather than compiling a generated script every display frame.
const __bands=new Float32Array(__audioSamples,0,32);
const __wave=new Float32Array(__audioSamples,32*4,256);
const __signalMethods={
  bandAt(b,age=0){return __past(b==='amp'?'level':b,age,this);},
  eventAt(k,age=0){let s=__pastSignal(age,this);return {n:s.beat,age:(s[k+'Age']??30)+age};},
  spectrumAt(x){return __bands[Math.min(31,Math.max(0,Math.floor(x*31)))];},
  levelAt(age=0){return __past('level',age,this);},
  hitAt(age=0){return __past('hit',age,this);},
  sampleAt(age=0){return __wave[Math.max(0,Math.min(255,255-Math.floor(age*48000/4)))];}
};
globalThis.__nativeFrame=function(ms,low,mid,high,level,beat,kickAge,snareAge){
  const signal=Object.assign(Object.create(__signalMethods),{
    active:true,source:'cd',mode:'play',low,mid,high,level,amp:level,
    hit:Math.exp(-kickAge*9),beat,kickAge,snareAge,hatAge:snareAge,songTime:ms/1000
  });
  __tick(ms,signal);
};
