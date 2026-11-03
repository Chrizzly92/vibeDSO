// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "measurements.hpp"
namespace dso {
struct StreamStats {
 uint32_t count=0; uint64_t sum=0,squares=0; uint16_t minimum=4095,maximum=0;
 void add(uint16_t s) { ++count; sum+=s; squares+=uint32_t(s)*s; if(s<minimum) minimum=s; if(s>maximum) maximum=s; }
};
struct StreamView {
 StreamStats stats; uint64_t total=0,last_period=0,period_sum=0,high_sum=0,min_period=~uint64_t(0),max_period=0; unsigned periods=0;
};
// IRQ-side integer accumulators only. No per-sample division or display work.
class StreamMeter {
 StreamStats current_,previous_window_;
 uint32_t window_=100000; uint64_t total_=0,last_period_=0;
 uint16_t minimum_=4095,maximum_=0,previous_=2048;
 int middle_=2048,low_=2046,high_=2050,state_=-1;
 bool levels_=false,rise_pending_=false,fall_pending_=false,have_rise_=false,have_fall_=false;
 uint64_t candidate_rise_=0,candidate_fall_=0,last_rise_=0,last_fall_=0;
 uint64_t periods_[4]{},highs_[4]{}; unsigned used_=0,next_=0;
 void edge(uint16_t s) {
  if(s<minimum_) minimum_=s;
  if(s>maximum_) maximum_=s;
  const int span=maximum_-minimum_,mid=(int(maximum_)+minimum_)/2;
  const int tolerance=span/32>2?span/32:2;
  if(span>=16 && (!levels_ || mid-middle_>tolerance || middle_-mid>tolerance)) {
   middle_=mid; const int h=span/16>2?span/16:2; low_=mid-h; high_=mid+h;
   state_=-1; have_rise_=have_fall_=rise_pending_=fall_pending_=false; used_=next_=0; levels_=true;
  }
  if(!levels_) return;
  if(state_<0) { if(s<=low_) state_=0; else if(s>=high_) state_=1; return; }
  auto position=[&]() { return edge_q16(unsigned(1),previous_,s,middle_)+(total_-1)*65536; };
  if(state_==0) {
   if(previous_<middle_ && s>=middle_) { candidate_rise_=position(); rise_pending_=true; }
   if(s>=high_ && rise_pending_) {
    if(have_rise_ && have_fall_ && last_fall_>last_rise_ && last_fall_<candidate_rise_) {
     periods_[next_]=candidate_rise_-last_rise_; highs_[next_]=last_fall_-last_rise_;
     next_=(next_+1)&3; if(used_<4) ++used_; last_period_=total_;
    }
    last_rise_=candidate_rise_; have_rise_=true; rise_pending_=false; state_=1;
   }
  } else {
   if(previous_>middle_ && s<=middle_) { candidate_fall_=position(); fall_pending_=true; }
   if(s<=low_ && fall_pending_) {
    last_fall_=candidate_fall_; have_fall_=true; fall_pending_=false; state_=0;
   }
  }
 }
public:
 void reset(uint32_t window=100000) { *this=StreamMeter{}; window_=window<100000?100000:window; }
 void push(uint16_t s) {
  current_.add(s); edge(s); previous_=s; ++total_;
  if(current_.count>=window_) {
   if(used_ && window_>=(periods_[(next_+3)&3]>>16) && current_.maximum-current_.minimum>=16) {
    minimum_=current_.minimum; maximum_=current_.maximum;
   }
   previous_window_=current_; current_=StreamStats{};
  }
 }
 int midpoint() const {
  unsigned lo=current_.count?current_.minimum:4095,hi=current_.count?current_.maximum:0;
  if(previous_window_.count) { if(previous_window_.minimum<lo) lo=previous_window_.minimum; if(previous_window_.maximum>hi) hi=previous_window_.maximum; }
  return current_.count || previous_window_.count?int((lo+hi+1)/2):previous_;
 }
 StreamView view() const {
  StreamView v; v.stats=current_;
  if(previous_window_.count) {
   v.stats.count+=previous_window_.count; v.stats.sum+=previous_window_.sum; v.stats.squares+=previous_window_.squares;
   if(previous_window_.minimum<v.stats.minimum) v.stats.minimum=previous_window_.minimum;
   if(previous_window_.maximum>v.stats.maximum) v.stats.maximum=previous_window_.maximum;
  }
  v.total=total_; v.last_period=last_period_; v.periods=used_;
  for(unsigned i=0;i<used_;++i) {
   const auto p=periods_[i]; v.period_sum+=p; v.high_sum+=highs_[i];
   if(p<v.min_period) v.min_period=p;
   if(p>v.max_period) v.max_period=p;
  }
  return v;
 }
};
inline Measurements measure_stream(const StreamView& v,uint64_t ticks,unsigned sensitivity,int zero) {
 Measurements m; const auto& s=v.stats;
 if(s.count<2 || !ticks || sensitivity<4 || sensitivity>12) return m;
 m.valid=true; m.minimum=s.minimum; m.maximum=s.maximum; m.clipped=s.minimum<=2 || s.maximum>=4093;
 m.mean_adc_q8=uint32_t((s.sum*256+s.count/2)/s.count);
 m.minimum_uv=signed_codes_to_uv((int(s.minimum)-zero)*256,sensitivity);
 m.maximum_uv=signed_codes_to_uv((int(s.maximum)-zero)*256,sensitivity);
 m.mean_uv=signed_codes_to_uv(int32_t(m.mean_adc_q8)-zero*256,sensitivity);
 m.pp_uv=codes_q8_to_uv((s.maximum-s.minimum)*256,sensitivity);
 const int64_t squares=int64_t(s.squares)-2LL*zero*int64_t(s.sum)+int64_t(s.count)*zero*zero;
 const uint64_t mean=uint64_t(squares)/s.count,remainder=uint64_t(squares)%s.count;
 m.rms_uv=codes_q8_to_uv(uint32_t(integer_sqrt(mean*65536+remainder*65536/s.count)),sensitivity);
 if(!v.periods || m.clipped) return m;
 const auto p=v.period_sum/v.periods;
 m.too_fast=v.min_period<8*65536; m.periods=v.periods;
 const uint64_t timeout=(p>>16)*3>200000?(p>>16)*3:200000;
 if(m.too_fast || v.max_period-v.min_period>p/4 || v.total-v.last_period>timeout || ticks>~uint64_t(0)/p) return m;
 m.frequency_uhz=uint64_t(board::core_hz)*1000000*65536/(ticks*p);
 m.duty_permille=uint16_t((v.high_sum*1000+v.period_sum/2)/v.period_sum);
 m.period_ns=ticks_q16_to_ns(p,ticks); m.high_ns=ticks_q16_to_ns(v.high_sum/v.periods,ticks); m.timing_valid=true;
 return m;
}
}
