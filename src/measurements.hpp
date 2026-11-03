// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "vertical_calibration.hpp"
#include "clock_profile.hpp"

namespace dso {
constexpr uint32_t sensitivity_uv[] = {5000000,2000000,1000000,500000,200000,100000,50000,20000,10000};
struct Measurements {
 uint32_t pp_uv=0,rms_uv=0,mean_adc_q8=0;
 uint64_t frequency_uhz=0,period_ns=0,high_ns=0;
 int32_t minimum_uv=0,maximum_uv=0,mean_uv=0;
 uint16_t duty_permille=0,minimum=0,maximum=0,periods=0;
 bool valid=false,clipped=false,timing_valid=false,too_fast=false;
};
inline uint64_t integer_sqrt(uint64_t n) {
 uint64_t result=0,bit=uint64_t(1)<<62;
 while(bit>n) bit>>=2;
 while(bit) {
  if(n>=result+bit) { n-=result+bit; result=(result>>1)+bit; }
  else result>>=1;
  bit>>=2;
 }
 return result;
}
inline uint32_t codes_q8_to_uv(uint32_t codes,unsigned sensitivity) {
 if(sensitivity<4 || sensitivity>12) return 0;
 const uint64_t numerator=uint64_t(codes)*sensitivity_uv[sensitivity-4]*275*vertical_gain(sensitivity);
 constexpr uint64_t denominator=uint64_t(1092)*1024*25*256;
 return uint32_t((numerator+denominator/2)/denominator);
}
inline int32_t signed_codes_to_uv(int32_t q8,unsigned sensitivity) {
 return q8<0?-int32_t(codes_q8_to_uv(uint32_t(-q8),sensitivity)):int32_t(codes_q8_to_uv(uint32_t(q8),sensitivity));
}
inline uint64_t ticks_q16_to_ns(uint64_t q16,uint64_t ticks) {
 const uint64_t product=q16*ticks,denominator=uint64_t(board::core_hz)*65536;
 const uint64_t remainder=product%denominator;
 return (product/denominator)*1000000000ULL+((remainder/65536)*1000000000ULL)/board::core_hz+
        ((remainder%65536)*1000000000ULL)/denominator;
}
inline uint64_t edge_q16(unsigned index,int previous,int now,int middle) {
 const int difference=now-previous;
 const int fraction=difference?((middle-previous)*65536)/difference:0;
 return uint64_t(index-1)*65536+uint32_t(fraction);
}
inline Measurements measure(const uint16_t* samples,unsigned count,uint64_t ticks,unsigned sensitivity,int zero) {
 Measurements m;
 if(!samples || count<2 || count>1024 || !ticks || sensitivity<4 || sensitivity>12 || zero<0 || zero>4095) return m;
 uint64_t sum=0,squares=0;
 m.minimum=4095;
 for(unsigned i=0;i<count;++i) {
  const unsigned s=samples[i];
  if(s>4095) return Measurements{};
  if(s<m.minimum) m.minimum=s;
  if(s>m.maximum) m.maximum=s;
  const int difference=int(s)-zero;
  sum+=s; squares+=uint64_t(int64_t(difference)*difference);
 }
 m.valid=true; m.clipped=m.minimum<=2 || m.maximum>=4093;
 m.mean_adc_q8=uint32_t((sum*256+count/2)/count);
 m.minimum_uv=signed_codes_to_uv((int(m.minimum)-zero)*256,sensitivity);
 m.maximum_uv=signed_codes_to_uv((int(m.maximum)-zero)*256,sensitivity);
 m.mean_uv=signed_codes_to_uv(int32_t(m.mean_adc_q8)-zero*256,sensitivity);
 m.pp_uv=codes_q8_to_uv(uint32_t(m.maximum-m.minimum)*256,sensitivity);
 const uint64_t mean_square_q16=(squares/count)*65536+((squares%count)*65536)/count;
 m.rms_uv=codes_q8_to_uv(uint32_t(integer_sqrt(mean_square_q16)),sensitivity);
 const int span=m.maximum-m.minimum;
 if(span<16) return m; // Too little swing for robust edge measurements.
 const int middle=(int(m.maximum)+m.minimum)/2;
 const int hysteresis=span/16>2?span/16:2;
 const int low=middle-hysteresis,high=middle+hysteresis;
 int state=samples[0]<=low?0:samples[0]>=high?1:-1;
 uint64_t candidate_rise=0,candidate_fall=0,last_rise=0,last_fall=0;
 uint64_t period_sum=0,high_sum=0,min_period=~uint64_t(0),max_period=0;
 bool rise_pending=false,fall_pending=false,have_rise=false,have_fall=false;
 auto add_period=[&](uint64_t period,uint64_t high_time) {
  period_sum+=period; high_sum+=high_time; ++m.periods;
  if(period<min_period) min_period=period;
  if(period>max_period) max_period=period;
 };
 for(unsigned i=1;i<count;++i) {
  const int p=samples[i-1],s=samples[i];
  if(state<0) { if(s<=low) state=0; else if(s>=high) state=1; continue; }
  if(state==0) {
   if(p<middle && s>=middle) { candidate_rise=edge_q16(i,p,s,middle); rise_pending=true; }
   if(s>=high && rise_pending) {
    if(have_rise && have_fall && last_fall>last_rise && last_fall<candidate_rise) {
     add_period(candidate_rise-last_rise,last_fall-last_rise);
    }
    last_rise=candidate_rise; have_rise=true;
    rise_pending=false; state=1;
   }
  } else {
   if(p>middle && s<=middle) { candidate_fall=edge_q16(i,p,s,middle); fall_pending=true; }
   if(s<=low && fall_pending) {
    if(have_fall && have_rise && last_rise>last_fall && last_rise<candidate_fall)
     add_period(candidate_fall-last_fall,candidate_fall-last_rise);
    last_fall=candidate_fall; have_fall=true; fall_pending=false; state=0;
   }
  }
 }
 if(!m.periods) return m;
 const uint64_t period_q16=period_sum/m.periods;
 m.too_fast=min_period<8*65536;
 if(m.too_fast || max_period-min_period>period_q16/4 || m.clipped) return m;
 // APB1 timers run at HCLK; continuous ADC intervals use the same tick unit.
 if(ticks>~uint64_t(0)/period_q16) return m;
 const uint64_t denominator=ticks*period_q16;
 m.frequency_uhz=(uint64_t(board::core_hz)*1000000*65536)/denominator;
 m.duty_permille=uint16_t((high_sum*1000+period_sum/2)/period_sum);
 m.period_ns=ticks_q16_to_ns(period_q16,ticks);
 m.high_ns=ticks_q16_to_ns(high_sum/m.periods,ticks);
 m.timing_valid=true;
 return m;
}

inline void format_fixed(char out[11],const char* prefix,uint64_t value,unsigned decimals,const char* unit) {
 char reverse[24]; unsigned n=0,pos=0;
 const uint64_t scale=decimals==2?100:decimals==1?10:1;
 uint64_t whole=value/scale;
 do { reverse[n++]=char('0'+whole%10); whole/=10; } while(whole);
 auto put=[&](char c) { if(pos<10) out[pos]=c; ++pos; };
 while(*prefix) put(*prefix++);
 while(n) put(reverse[--n]);
 if(decimals) {
  put('.');
  if(decimals==2) put(char('0'+(value%100)/10));
  put(char('0'+value%10));
 }
 while(*unit) put(*unit++);
 if(pos>10) { out[0]='O'; out[1]='V'; out[2]='E'; out[3]='R'; pos=4; }
 out[pos]=0;
}
inline void format_voltage(char out[11],const char* prefix,uint32_t uv) {
 if(uv>=1000000) format_fixed(out,prefix,(uint64_t(uv)+5000)/10000,2,"V");
 else if(uv>=1000) format_fixed(out,prefix,(uv+50)/100,1,"mV");
 else format_fixed(out,prefix,uv,0,"uV");
}
inline void format_frequency(char out[11],uint64_t uhz) {
 if(uhz>=1000000000) format_fixed(out,"F",(uhz+5000000)/10000000,2,"kHz");
 else if(uhz>=1000000) format_fixed(out,"F",(uhz+5000)/10000,2,"Hz");
 else if(uhz>=1000) format_fixed(out,"F",(uhz+5)/10,2,"mHz");
 else format_fixed(out,"F",uhz,0,"uHz");
}
}

namespace dso {
inline void compact_quantity(char out[11],const char* label,int64_t value,bool time=false) {
 const bool negative=value<0;
 const uint64_t magnitude=negative?uint64_t(-value):uint64_t(value);
 uint64_t scale=time?1000000000ULL:1000000ULL;
 const char* unit=time?"s":"V";
 if(magnitude<scale) { scale=time?1000000:1000; unit=time?"ms":"mV"; }
 if(magnitude<scale) { scale=time?1000:1; unit=time?"us":"uV"; }
 if(time && magnitude<scale) { scale=1; unit="ns"; }
 char prefix[6]; unsigned pos=0; while(label[pos] && pos<4) { prefix[pos]=label[pos]; ++pos; }
 if(negative) prefix[pos++]='-';
 prefix[pos]=0;
 for(int decimals=2;decimals>=0;--decimals) {
  const unsigned factor=decimals==2?100:decimals==1?10:1;
  format_fixed(out,prefix,(magnitude*factor+scale/2)/scale,decimals,unit);
  unsigned n=0; while(out[n]) ++n;
  if(n<=8 && out[0]!='O') return;
 }
 out[0]='-'; out[1]='-'; out[2]=0;
}
}
