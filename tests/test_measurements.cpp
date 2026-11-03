// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "../src/measurements.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
unsigned checks=0;
void check(bool pass,const char* message) {
 if(!pass) { printf("FAIL %s\n",message); exit(1); }
 ++checks; printf("PASS %s\n",message);
}
bool near(uint64_t value,uint64_t expected,uint64_t error) {
 return value>expected?value-expected<=error:expected-value<=error;
}
int main() {
 uint16_t data[1024];
 for(auto& x:data) x=2520;
 auto m=dso::measure(data,1024,560,6,2048);
 check(m.valid && !m.timing_valid && m.pp_uv==0,"DC produces zero Vpp and no fictitious frequency");
 check(near(m.rms_uv,5000000,15000),"5 V calibrated DC produces approximately 5 V total RMS");
 check(m.mean_adc_q8==2520*256,"diagnostic ADC mean retains fractional-code precision");
 check(near(dso::codes_q8_to_uv(404*256,5),8500000,20000),"2 V/div voltage conversion matches accepted 2 V/div reference fit");
 check(near(dso::codes_q8_to_uv(472*256,6),5000000,15000),"1 V/div voltage conversion matches existing 5 V trim");
 const unsigned duties[]={25,50,75}; for(unsigned duty: duties) {
  for(unsigned i=0;i<1000;++i) data[i]=(i%100>=10 && i%100<10+duty)?2248:2048;
  m=dso::measure(data,1000,560,6,2048);
  check(m.timing_valid && near(m.frequency_uhz,1000000000,10),"1 kHz square frequency uses full-period interpolated crossings");
  check(near(m.duty_permille,duty*10,1),"square duty uses only complete rise-fall-rise periods");
  check(near(m.rms_uv,uint64_t(m.pp_uv*std::sqrt(duty/100.0)),100),"square total RMS includes the DC component");
 }
 for(unsigned i=0;i<1000;++i) data[i]=uint16_t(std::lround(2048+200*std::sin(2*3.141592653589793*i/100)));
 m=dso::measure(data,1000,560,6,2048);
 check(m.timing_valid && near(m.frequency_uhz,1000000000,20000),"sine wave frequency is independent of square-wave duty assumption");
 check(near(m.duty_permille,500,2),"sine duty is the time above its mid-amplitude threshold");
 check(near(m.rms_uv,uint64_t(m.pp_uv/(2*std::sqrt(2.0))),2000),"sine RMS agrees with analytical peak-to-peak relationship");
 const auto old_pp=m.pp_uv,old_rms=m.rms_uv;
 for(unsigned i=0;i<1000;++i) data[i]+=180;
 auto shifted=dso::measure(data,1000,560,6,2228);
 check(shifted.pp_uv==old_pp && shifted.rms_uv==old_rms,"zero calibration removes ADC bias without changing signal amplitude");
 auto slow=dso::measure(data,1000,560000000,6,2228);
 check(slow.timing_valid && near(slow.frequency_uhz,1000,1),"slow timebases retain sub-Hz frequency without integer overflow");
 auto short_record=dso::measure(data,30,560,6,2228);
 check(!short_record.timing_valid,"incomplete cycles suppress frequency and duty");
 for(unsigned i=0;i<1024;++i) data[i]=(i+1000-512)%1000<500?2248:2048;
 m=dso::measure(data,1024,56,6,2048);
 check(m.timing_valid && m.frequency_uhz==1000000000 && m.duty_permille==500,"1 kHz centered trigger at 1 MS/s uses fall-rise-fall when only one rising edge fits");
 for(unsigned i=0;i<1024;++i) data[i]=4096-data[i];
 m=dso::measure(data,1024,56,6,2048);
 check(m.timing_valid && m.frequency_uhz==1000000000 && m.duty_permille==500,"period selection remains valid after reversing trigger polarity");
 for(unsigned i=0;i<1024;++i) data[i]=2048+(i%7)-3;
 m=dso::measure(data,1024,560,6,2048);
 check(m.valid && !m.timing_valid,"small baseline noise cannot manufacture an edge frequency");
 for(unsigned i=0;i<1024;++i) data[i]=(i%4<2)?2200:2000;
 m=dso::measure(data,1024,56,6,2048);
 check(m.too_fast && !m.timing_valid,"fewer than eight samples per period are flagged FAST");
 for(unsigned i=0;i<1024;++i) data[i]=(i%100<50)?4095:0;
 m=dso::measure(data,1024,560,6,2048);
 check(m.clipped && !m.timing_valid,"ADC rail clipping invalidates displayed timing and voltage claims");
 for(unsigned i=0;i<1024;++i) data[i]=2048+((i%200<20 || (i%200>=80 && i%200<150))?200:0);
 m=dso::measure(data,1024,560,6,2048);
 check(!m.timing_valid,"inconsistent edge spacing suppresses a misleading periodic frequency");
 for(unsigned i=0;i<1000;++i) data[i]=uint16_t(2048+(i%100<50?200:0)+int(i%7)-3);
 m=dso::measure(data,1000,560,6,2048);
 check(m.timing_valid && near(m.frequency_uhz,1000000000,1000000) && near(m.duty_permille,500,2),"Schmitt arming tolerates small plateau noise without extra periods");
 for(unsigned i=0;i<1024;++i) data[i]=4095;
 m=dso::measure(data,1024,1120000000,4,0);
 check(m.valid && m.rms_uv<300000000 && m.rms_uv>200000000,"worst-case RMS and voltage arithmetic stays within bounds");
 check(!dso::measure(nullptr,1024,560,6,2048).valid && !dso::measure(data,0,560,6,2048).valid && !dso::measure(data,1025,560,6,2048).valid,"invalid record bounds are rejected");
 check(!dso::measure(data,1024,0,6,2048).valid && !dso::measure(data,1024,560,3,2048).valid,"invalid timebase and sensitivity are rejected");
 data[3]=5000;
 check(!dso::measure(data,1024,560,6,2048).valid,"non-ADC sample data is rejected");
 // Same three-code extremum expansion, different input volts per ADC code.
 const auto pp_5v=dso::codes_q8_to_uv(63*256,4);
 const auto noisy_5v=dso::codes_q8_to_uv(66*256,4);
 const auto pp_1v=dso::codes_q8_to_uv(312*256,6);
 const auto noisy_1v=dso::codes_q8_to_uv(315*256,6);
 check(noisy_5v-pp_5v>150000 && noisy_5v-pp_5v<160000,"three ADC codes of extrema noise can add about 157 mV at 5 V/div");
 check(noisy_1v-pp_1v>30000 && noisy_1v-pp_1v<33000,"same ADC extrema noise adds only about 32 mV at 1 V/div");
 for(unsigned i=0;i<1024;++i) data[i]=i%100<50?2137:2074;
 const auto raw_pp=dso::measure(data,1024,560,4,2074).pp_uv;
 data[200]=2073; data[301]=2139;
 check(dso::measure(data,1024,560,4,2074).pp_uv>raw_pp+150000,"Vpp intentionally includes isolated captured extrema rather than silently suppressing overshoot");
 char text[11];
 dso::format_voltage(text,"PP",8500000); check(!strcmp(text,"PP8.50V"),"Vpp formatting uses volts");
 dso::format_voltage(text,"R",20400); check(!strcmp(text,"R20.4mV"),"RMS formatting uses millivolts");
 dso::format_voltage(text,"PP",800); check(!strcmp(text,"PP800uV"),"small voltages retain microvolt units");
 dso::format_frequency(text,1000000000); check(!strcmp(text,"F1.00kHz"),"frequency formatting uses kHz");
 dso::format_frequency(text,500000); check(!strcmp(text,"F500.00mHz"),"sub-Hz frequency formatting uses mHz");
 dso::format_fixed(text,"D",500,1,"%"); check(!strcmp(text,"D50.0%"),"duty formatting preserves one decimal place");
 dso::format_fixed(text,"A",22281,1,""); check(!strcmp(text,"A2228.1"),"diagnostic mean formatting retains one decimal place");
 bool roots=true;
 for(uint64_t value=0;value<100000;value+=37) {
  const auto root=dso::integer_sqrt(value);
  if(root*root>value || (root+1)*(root+1)<=value) roots=false;
 }
 check(roots,"integer RMS square root satisfies exact bounding property");

 uint16_t asymmetric[1000]; for(unsigned i=0;i<1000;++i) asymmetric[i]=i%100<25?2300:1900;
 const auto extra=dso::measure(asymmetric,1000,board::core_hz/100000,6,2048);
 check(extra.timing_valid && extra.period_ns==1000000 && extra.high_ns==250000,"extended measurements retain actual full period and positive pulse duration");
 check(extra.minimum_uv<0 && extra.maximum_uv>0 && extra.mean_uv<0 && extra.mean_adc_q8==2000*256,"signed voltage extrema and mean respect calibrated zero and unequal duty");
 check(dso::ticks_q16_to_ns(500ULL*65536,board::core_hz*20ULL)==10000000000000ULL,"long-period conversion avoids overflowing nanosecond intermediates");
 bool compact=true; const int64_t values[]={-125000,3300000,-500,999999,0,200000000};
 for(auto value:values) { dso::compact_quantity(text,"Mn",value); compact &= strlen(text)<=8 && text[0]=='M'; }
 check(compact,"compact signed voltage formats fit their eight-character fields across ranges");
 dso::compact_quantity(text,"PW",250000,true); check(!strcmp(text,"PW250us"),"compact pulse duration keeps explicit time units");
 printf("%u checks passed (measurement algorithms and compact formatting).\n",checks);
}

