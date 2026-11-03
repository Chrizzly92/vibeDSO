// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "clock_profile.hpp"
#include "remote_protocol.hpp"
namespace dso {
struct ExportFrame {
 const uint16_t* samples=nullptr; const volatile uint16_t* maximum=nullptr;
 uint64_t ticks=0; uint32_t sequence=0,published_ms=0;
 uint16_t count=0,flags=0,zero=2048,gain=1024;
 uint8_t sensitivity=4,coupling=0;
};
struct MirrorText {
 uint16_t x=0,color=0; uint8_t y=0,width=0,selected=0,length=0; char chars[10]{};
};
struct MirrorView {
 ExportFrame frame;
 const int16_t* top=nullptr; const int16_t* bottom=nullptr;
 int16_t zero_y=117,trigger_y=117,bar_left=85,bar_right=235;
 uint8_t focus=0,edge=0,generator=0,measurements=0,cursors=0,status=0,hp_selected=0;
 uint8_t cursor_flags=0,cursor_selected=0; uint16_t cursor_x[2]{},cursor_y[2]{};
 uint16_t reference_color=0,power_color=0; char power[7]{};
 MirrorText texts[9]{}; uint8_t text_count=0,clip_flags=0;
};
inline uint32_t crc32(const uint8_t* p,unsigned n) {
 uint32_t crc=0xffffffff;
 for(unsigned i=0;i<n;++i) { crc^=p[i]; for(unsigned j=0;j<8;++j) crc=(crc>>1)^(0xedb88320U&uint32_t(-int(crc&1))); }
 return ~crc;
}
class FrameExporter {
 uint8_t packet_[2100]{}; char command_[40]{}; unsigned used_=0; bool overflow_=false;
 ControlHandler control_handler_=nullptr; ControlState control_{}; bool control_pending_=false;
 bool snapshot_=false,stream_=false,mirror_=false; unsigned hz_=0;
 uint32_t last_ms_=0,last_sequence_=0; unsigned size_=0;
 void put16(unsigned at,uint16_t v) { packet_[at]=uint8_t(v); packet_[at+1]=uint8_t(v>>8); }
 void put32(unsigned at,uint32_t v) { put16(at,uint16_t(v)); put16(at+2,uint16_t(v>>16)); }
 bool equals(const char* text) const { unsigned i=0; for(;text[i] && command_[i]==text[i];++i) {} return !text[i] && !command_[i]; }
 bool rate_command(const char* name,unsigned fallback,unsigned limit,bool mirror) {
  unsigned n=0; while(name[n] && command_[n]==name[n]) ++n;
  if(name[n]) return false;
  unsigned rate=fallback;
  if(command_[n]) {
   if(command_[n++]!=' ') return false;
   rate=0; unsigned digits=0;
   while(command_[n]>='0' && command_[n]<='9' && digits<2) { rate=rate*10+command_[n++]-'0'; ++digits; }
   if(!digits || command_[n] || !rate || rate>limit) return false;
  }
  stream_=true; snapshot_=false; hz_=rate;
  if(mirror_!=mirror) last_sequence_=0;
  mirror_=mirror; return true;
 }
 void dispatch() {
  if(command_[0]=='@') {
   if(control_pending_) return; // PC keeps only one request in flight.
   unsigned at=1; uint32_t id=0;
   while(command_[at]>='0' && command_[at]<='9' && at<=5) id=id*10+command_[at++]-'0';
   if(at==1 || command_[at]!=' ' || !id || id>65535) return;
   control_=ControlState{}; control_.request=uint16_t(id);
   if(control_handler_) control_handler_(command_+at+1,&control_); else control_.result=1;
   control_pending_=true; return;
  }
  if(equals("SNAP") || equals("VIEW")) {
   const bool mirror=equals("VIEW"); if(mirror_!=mirror) last_sequence_=0;
   mirror_=mirror; snapshot_=true; stream_=false;
  } else if(equals("STOP")) { stream_=snapshot_=false; }
  else if(!rate_command("STREAM",4,5,false)) rate_command("MIRROR",20,20,true);
 }
 void header(const ExportFrame& f,uint8_t kind,unsigned count,unsigned payload,uint64_t ticks,uint16_t flags) {
  packet_[0]='D'; packet_[1]='S'; packet_[2]='O'; packet_[3]='1'; packet_[4]=1; packet_[5]=kind;
  put16(6,48); put32(8,f.sequence); put32(12,f.published_ms); put32(16,uint32_t(ticks)); put32(20,uint32_t(ticks>>32));
  put32(24,board::core_hz); put16(28,count); put16(30,flags); put16(32,f.zero);
  packet_[34]=f.sensitivity; packet_[35]=f.coupling; put16(36,f.gain); put16(38,f.count);
  put32(40,last_sequence_ && f.sequence>last_sequence_?f.sequence-last_sequence_-1:0); put16(44,payload); put16(46,0);
 }
 void finish(const ExportFrame& f,unsigned payload,uint32_t ms) {
  put32(48+payload,crc32(packet_,48+payload)); size_=52+payload;
  last_ms_=ms; last_sequence_=f.sequence; snapshot_=false;
 }

public:
 void set_control_handler(ControlHandler handler) { control_handler_=handler; }
 bool prepare_control(uint32_t ms) {
  if(!control_pending_) return false;
  ExportFrame f; f.sequence=control_.request; f.published_ms=ms;
  header(f,4,0,32,0,0); const unsigned o=48; const auto& c=control_;
  packet_[o]=1; packet_[o+1]=c.result; put16(o+2,c.request);
  packet_[o+4]=c.timebase; packet_[o+5]=c.mode; packet_[o+6]=c.edge; packet_[o+7]=c.focus;
  packet_[o+8]=c.sensitivity; packet_[o+9]=c.coupling; put16(o+10,c.flags);
  put16(o+12,uint16_t(c.vpos)); put16(o+14,c.hpos); put16(o+16,uint16_t(c.trigger)); put16(o+18,c.reclen);
  put32(o+20,c.generator); put16(o+24,uint16_t(c.zero)); put16(o+26,c.cursor_a); put16(o+28,c.cursor_b); put16(o+30,0);
  put32(o+32,crc32(packet_,o+32)); size_=84; control_pending_=false; return true;
 }
 void discard_command() { used_=0; overflow_=true; }
 void receive(uint8_t byte) {
  if(byte=='\r') return;
  if(byte=='\n') { if(!overflow_) { command_[used_]=0; dispatch(); } used_=0; overflow_=false; return; }
  if(used_+1<sizeof(command_)) command_[used_++]=char(byte); else overflow_=true;
 }
 bool mirror() const { return mirror_; }
 bool wants(uint32_t ms) const { return snapshot_ || (stream_ && uint32_t(ms-last_ms_)>=1000/hz_); }
 bool prepare(const ExportFrame& f,uint32_t ms) {
  if(mirror_ || !wants(ms) || !f.samples || f.count<2 || f.count>1024 || !f.ticks || f.sensitivity<4 || f.sensitivity>12) return false;
  if(!snapshot_ && f.sequence==last_sequence_) return false;
  const bool peak=f.maximum!=nullptr; const unsigned stride=peak && f.count>512?2:1;
  const unsigned count=(f.count+stride-1)/stride,payload=count*(peak?4:2);
  if(payload>2048) return false;
  header(f,peak?2:1,count,payload,f.ticks*stride,f.flags|(stride>1?16:0));
  for(unsigned i=0;i<count;++i) {
   const unsigned at=i*stride; uint16_t lo=f.samples[at],hi=peak?f.maximum[at]:lo;
   if(stride==2 && at+1<f.count) { if(f.samples[at+1]<lo) lo=f.samples[at+1]; if(f.maximum[at+1]>hi) hi=f.maximum[at+1]; }
   put16(48+i*(peak?4:2),lo); if(peak) put16(50+i*4,hi);
  }
  finish(f,payload,ms); return true;
 }
 bool prepare_mirror(const MirrorView& v,uint32_t ms) {
  const auto& f=v.frame;
  if(!mirror_ || !wants(ms) || !v.top || !v.bottom || !f.ticks || v.text_count>9 || f.sensitivity<4 || f.sensitivity>12) return false;
  if(!snapshot_ && f.sequence==last_sequence_ && uint32_t(ms-last_ms_)<1000) return false;
  unsigned payload=640;
  for(unsigned i=0;i<v.text_count;++i) {
   const auto& t=v.texts[i]; if(t.length>10 || !t.width || unsigned(t.x)+t.width>320 || t.y>224) return false;
   payload+=8+t.length;
  }
  for(unsigned i=0;i<300;++i) if(v.top[i]<17 || v.bottom[i]>216 || v.top[i]>v.bottom[i]) return false;
  header(f,3,300,payload,f.ticks,f.flags);
  const unsigned o=48;
  packet_[o]=4; packet_[o+1]=v.focus; packet_[o+2]=v.edge; packet_[o+3]=v.generator;
  packet_[o+4]=v.measurements; packet_[o+5]=v.cursors; packet_[o+6]=v.status; packet_[o+7]=v.hp_selected;
  put16(o+8,uint16_t(v.zero_y)); put16(o+10,uint16_t(v.trigger_y)); put16(o+12,uint16_t(v.bar_left)); put16(o+14,uint16_t(v.bar_right));
  packet_[o+16]=v.cursor_flags; packet_[o+17]=v.cursor_selected;
  put16(o+18,v.cursor_x[0]); put16(o+20,v.cursor_x[1]); put16(o+22,v.cursor_y[0]); put16(o+24,v.cursor_y[1]);
  packet_[o+26]=v.text_count; packet_[o+27]=0; put16(o+28,v.reference_color); put16(o+30,v.power_color);
  for(unsigned i=0;i<7;++i) packet_[o+32+i]=uint8_t(v.power[i]);
  packet_[o+39]=v.clip_flags;
  for(unsigned i=0;i<300;++i) { packet_[o+40+i*2]=uint8_t(v.top[i]); packet_[o+41+i*2]=uint8_t(v.bottom[i]); }
  unsigned at=o+640;
  for(unsigned i=0;i<v.text_count;++i) {
   const auto& t=v.texts[i]; put16(at,t.x); packet_[at+2]=t.y; packet_[at+3]=t.width; put16(at+4,t.color);
   packet_[at+6]=t.selected; packet_[at+7]=t.length; at+=8;
   for(unsigned j=0;j<t.length;++j) packet_[at++]=uint8_t(t.chars[j]);
  }
  finish(f,payload,ms); return true;
 }

 const uint8_t* data() const { return packet_; }
 unsigned size() const { return size_; }
};
}
