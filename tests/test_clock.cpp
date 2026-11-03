// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "../src/clock_hardware.hpp"
#include "../src/engine_math.hpp"
#include "../src/test_signal.hpp"
#include "../src/acquisition_config.hpp"
#include <stdio.h>
#include <stdlib.h>
struct Rcc;
struct Cfgr {
 Rcc* owner; uint32_t raw=0;
 operator uint32_t() const { return raw; }
 Cfgr& operator=(uint32_t value);
 Cfgr& operator|=(uint32_t value) { return *this=raw|value; }
 Cfgr& operator&=(uint32_t value) { return *this=raw&value; }
};
struct Rcc {
 uint32_t CR=0; Cfgr CFGR{this};
};
Cfgr& Cfgr::operator=(uint32_t value) {
 constexpr uint32_t pll_fields=(15U<<18)|(3U<<16);
 if(owner->CR&(1U<<24)) value=(value&~pll_fields)|(raw&pll_fields);
 raw=(value&~12U)|(raw&12U); // SWS is read-only.
 return *this;
}
struct Flash { uint32_t ACR=0; };
struct Tick { uint32_t CTRL=0,LOAD=0,VAL=123; };
void poll(Rcc& r,bool hse=true,bool pll=true) {
 if(r.CR&1) r.CR|=2;
 if(hse && (r.CR&(1U<<16))) r.CR|=1U<<17;
 if(pll && (r.CR&(1U<<24)) && (r.CR&(1U<<17))) r.CR|=1U<<25;
 else r.CR&=~(1U<<25);
 const unsigned sw=r.CFGR.raw&3;
 if(sw==0 && (r.CR&2)) r.CFGR.raw&=~12U;
 if(sw==2 && (r.CR&(1U<<25))) r.CFGR.raw=(r.CFGR.raw&~12U)|8U;
}
unsigned checks=0;
void check(bool good,const char* label) {
 if(!good) { printf("FAIL %s\n",label); exit(1); }
 ++checks; printf("PASS %s\n",label);
}
int main() {
 Rcc r; Flash flash;
 check(board::configure_clock(r,flash,[&]{poll(r);},16),"clock initialization succeeds from reset with simulated readiness flags");
 check(((r.CFGR.raw>>18)&15)+2==board::pll_multiplier && (r.CFGR.raw&12)==8,"selected PLL multiplier and confirmed SWS match the active profile");
 check((flash.ACR&7)==2 && (flash.ACR&16),"flash wait states and prefetch remain valid at full core frequency");
 Rcc startup;
 startup.CR=3|(1U<<16)|(1U<<17)|(1U<<24)|(1U<<25);
 startup.CFGR.raw=(7U<<18)|(1U<<16)|(2U<<14)|(4U<<8)|10U;
 check(board::configure_clock(startup,flash,[&]{poll(startup);},16),"clock reconfiguration succeeds after SystemInit already enabled 72 MHz PLL");
 check(((startup.CFGR.raw>>18)&15)+2==board::pll_multiplier,"PLL write-protection model confirms HSI switch and PLL stop before multiplier change");
 Rcc no_hsi; Flash dummy;
 check(!board::configure_clock(no_hsi,dummy,[]{},4),"missing HSI readiness returns a bounded failure");
 Rcc no_hse;
 check(!board::configure_clock(no_hse,dummy,[&]{poll(no_hse,false);},4),"missing HSE readiness returns a bounded failure");
 Rcc no_pll;
 check(!board::configure_clock(no_pll,dummy,[&]{poll(no_pll,true,false);},4),"missing PLL lock returns a bounded failure");
 Tick tick; board::init_systick(tick);
 check(tick.CTRL==7 && tick.VAL==0 && (tick.LOAD+1)*1000==board::core_hz,"SysTick keeps exact 1 ms periods in this clock profile");
 check(board::test_timer_hz/(board::test_psc+1)/(board::test_arr+1)==1000,"internal test output remains 1 kHz in this clock profile");
 const double carrier=double(board::core_hz)/board::timer4_period;
 check(carrier>17575 && carrier<17580,"inverter carrier remains approximately 17.578 kHz");
 bool normalized=true;
 for(unsigned logical=0;logical<4096;++logical) {
  const unsigned compare=board::timer4_compare(logical);
  const double error=double(logical)/4096-double(compare)/board::timer4_period;
  if(compare>=board::timer4_period || error<0 || error>=1.0/board::timer4_period) normalized=false;
 }
 check(normalized,"all trigger and inverter commands preserve duty within one physical timer count");
 check(board::timer4_compare(2662)*100<board::timer4_period*65 && board::timer4_compare(2662)*100>board::timer4_period*64,"inverter reserve remains above 64 percent and below 65 percent");
 check(board::adc_hz<=14000000 && board::core_hz/2<=36000000,"ADC and APB1 clocks remain within device ratings");
#ifndef DSO_LEGACY_ENGINE
 check(board::core_hz==56000000 && board::adc_hz==14000000 && board::continuous_sample_ticks==56,"source profile provides exactly one microsecond per 14-cycle ADC conversion");
 bool safe=true;
 for(unsigned tb=1;tb<=24;++tb) {
  const auto t=dso::timing(tb);
  if(t.continuous) {
   if(tb!=23 && tb!=24) safe=false;
   if(board::core_hz/t.ticks!=1000000) safe=false;
  } else if(t.ticks<17*board::adc_divider) safe=false;
 }
 check(safe,"all 24 timebases use either continuous 1 MS/s or a timer interval above 17 ADC clocks");
 check((dso::continuous_dma_samples/2)*board::continuous_sample_ticks*1000000ULL/board::core_hz==2048,"direct DMA half-buffer deadline is 2048 microseconds at one megasample per second");
#else
 check(board::core_hz==72000000 && board::adc_hz==12000000 && board::timer4_period==4096,"legacy reference build keeps its previous 72 MHz and 12-bit PWM profile");
#endif
 printf("%u checks passed (clock readiness/write protection, timer rates and PWM scaling).\n",checks);
}
