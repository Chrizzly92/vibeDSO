// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "../src/test_signal.hpp"
#include "../src/measurements.hpp"
#include <stdio.h>
#include <stdlib.h>
struct Timer {
 uint32_t CR1=0,CR2=0,SMCR=0,DIER=0,SR=0,CCMR1=0,CCMR2=0,CCER=0;
 uint32_t CNT=0,PSC=0,ARR=0,CCR1=0,CCR2=0,CCR3=0,CCR4=0,DCR=0,DMAR=0,EGR=0;
};
unsigned checks=0;
void check(bool pass,const char* label) {
 if(!pass) { printf("FAIL %s\n",label); exit(1); }
 ++checks; printf("PASS %s\n",label);
}
// Timer-kernel-cycle model. Implements the actual configured mode, not a
// frequency formula that silently assumes PWM instead of toggle mode.
struct Output {
 Timer regs; uint32_t prescaled=0; bool toggled=false;
 bool value() const {
  const unsigned mode=(regs.CCMR1>>12)&7;
  bool high=mode==6?regs.CNT<regs.CCR2:mode==3?toggled:false;
  if(regs.CCER&(1U<<5)) high=!high;
  return (regs.CCER&(1U<<4)) && high;
 }
 void advance(unsigned cycles) {
  for(unsigned i=0;i<cycles;++i) {
   if(!(regs.CR1&1)) continue;
   if(++prescaled>regs.PSC) {
    prescaled=0;
    regs.CNT=regs.CNT>=regs.ARR?0:regs.CNT+1;
    if(((regs.CCMR1>>12)&7)==3 && regs.CNT==regs.CCR2) toggled=!toggled;
   }
  }
 }
};
int main() {
 Output output; board::init_test_signal(output.regs);
 check(((output.regs.CCMR1>>12)&7)==6,"shared firmware initializer selects PWM1 instead of toggle");
 check((output.regs.CCMR1&(1U<<11)) && output.regs.CCER==16,"test pin uses preloaded active-high channel 2");
 check(output.regs.PSC==board::core_hz/10000-1 && output.regs.ARR==9 && output.regs.CCR2==5,"register divisors and compare implement 100 us ticks and equal half periods");
 check(output.regs.EGR==1 && output.regs.CR1==129 && output.regs.SMCR==0,"timer starts after update with internal clock and edge-aligned counting");
 uint16_t samples[1024];
 for(auto& sample:samples) { sample=output.value()?2350:2048; output.advance(board::core_hz/100000); }
 auto measured=dso::measure(samples,1024,board::core_hz/100000,6,2048);
 check(measured.timing_valid && measured.frequency_uhz==1000000000,"generated register-model waveform measures 1 kHz through actual measurement algorithm");
 check(measured.duty_permille==500,"generated register-model waveform measures 50 percent duty");
 Output old; board::init_test_signal(old.regs); old.regs.CCMR1=3U<<12;
 for(auto& sample:samples) { sample=old.value()?2350:2048; old.advance(board::core_hz/100000); }
 measured=dso::measure(samples,1024,board::core_hz/100000,6,2048);
 check(measured.timing_valid && measured.frequency_uhz==500000000,"negative control reproduces old 500 Hz fault with identical divisors in toggle mode");

 bool bounds=true,accuracy=true,duties=true;
 for(auto hz:board::generator_frequencies) {
  const auto c=board::generator_timing(hz);
  const uint64_t actual=uint64_t(board::core_hz)*1000000/((uint64_t(c.psc)+1)*(uint64_t(c.arr)+1));
  bounds &= c.compare>0 && c.compare<=c.arr && actual==c.frequency_uhz;
  const uint64_t target=uint64_t(hz)*1000000;
  accuracy &= (actual>target?actual-target:target-actual)*10000<=target; // <=0.01 percent
  const int difference=int(c.compare)*2-int(c.arr)-1;
  duties &= difference>=-1 && difference<=1;
 }
 check(bounds,"every generator preset fits 16-bit timer registers and reports its applied frequency");
 check(accuracy,"all 1 Hz to 100 kHz presets stay within 0.01 percent nominal divider error");
 check(duties,"all generator presets have half-period compare within one timer tick");
 board::configure_generator(output.regs,2000,true);
 check(output.regs.ARR==board::core_hz/2000-1 && output.regs.CCR2==board::core_hz/4000 && ((output.regs.CCMR1>>12)&7)==6,
       "runtime frequency change uses PWM1 and refreshes period and compare together");
 board::configure_generator(output.regs,1000,false);
 check(output.regs.CCR2==0 && output.regs.CCER==16,"disabled output remains driven at constant low");
 check(board::generator_timing(0).frequency_uhz==board::generator_timing(1).frequency_uhz && board::generator_timing(100001).frequency_uhz==board::generator_timing(100000).frequency_uhz,
       "runtime generator clamps unsupported frequency requests");
 printf("%u checks passed (shared TIM3 initializer, cycle model and measurement pipeline).\n",checks);
}
