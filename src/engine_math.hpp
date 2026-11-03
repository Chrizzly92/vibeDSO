// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "vertical_calibration.hpp"
#include "clock_profile.hpp"

namespace dso {
// 25 px/div; 10 mV/div at the most sensitive switch position; U2C gain 8
// (R9=1k, R10=3k, R11=150 ohm), 12-bit ADC with nominal 3.3 V VDDA.
// pixels/code = 3300*25/(4095*10*8) = 275/1092.
constexpr int vertical_pixels(int adc_delta,unsigned sensitivity=0) {
 const int64_t scaled=int64_t(adc_delta)*275*vertical_gain(sensitivity);
 constexpr int denominator=1092*1024;
 return int((scaled+(scaled<0?-denominator/2:denominator/2))/denominator);
}
constexpr int adc_delta_for_pixels(int pixels,unsigned sensitivity) {
 const int64_t scaled=int64_t(pixels)*1092*1024;
 const int denominator=275*vertical_gain(sensitivity);
 return int((scaled+(scaled<0?-denominator/2:denominator/2))/denominator);
}
template<class T> constexpr T clamp(T v,T lo,T hi) { return v<lo?lo:v>hi?hi:v; }
// Original enum order, seconds/div represented in nanoseconds. 25 pixels/div.
constexpr uint64_t timebase_ns[] = {0,500000000000ULL,200000000000ULL,100000000000ULL,
 50000000000ULL,20000000000ULL,10000000000ULL,5000000000ULL,2000000000ULL,
 1000000000ULL,500000000ULL,200000000ULL,100000000ULL,50000000ULL,
 20000000ULL,10000000ULL,5000000ULL,2000000ULL,1000000ULL,500000ULL,
 200000ULL,100000ULL,50000ULL,20000ULL,10000ULL};
struct Timing { uint16_t psc,arr; uint64_t ticks; bool continuous; };
constexpr Timing timing(unsigned tb) {
 const uint64_t requested = timebase_ns[tb]*(board::core_hz/1000000)/25000ULL;
 if(requested<=board::continuous_sample_ticks)
  return {0,uint16_t(board::continuous_sample_ticks-1),board::continuous_sample_ticks,true};
 const uint64_t ticks = requested;
 const uint64_t divider = (ticks+65535)/65536;
 const uint64_t period = (ticks+divider/2)/divider;
 return {uint16_t(divider-1),uint16_t(period-1),divider*period,false};
}
constexpr bool crossing(uint16_t previous,uint16_t now,int threshold,bool rising) {
 return rising ? previous<threshold && now>=threshold : previous>threshold && now<=threshold;
}
constexpr uint16_t ring_index(uint16_t start,uint16_t offset) { return (start+offset)&1023; }
constexpr int64_t pixel_step_q16(unsigned tb,uint64_t ticks) {
 return int64_t(timebase_ns[tb]*(board::core_hz/1000000)*65536ULL/(25000ULL*ticks));
}
constexpr int64_t viewport_start_q16(int hpos,unsigned length,int64_t step,bool rolling) {
 const int64_t span=299*step;
 const int64_t maximum=int64_t(length-1)*65536-span;
 const int64_t desired=int64_t(hpos+150)*65536-150*step;
 return rolling ? (maximum>0?maximum:0) : clamp<int64_t>(desired,0,maximum>0?maximum:0);
}
}
