// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace boot {
constexpr uint32_t rom_base=0x1ffff000,rom_end=0x1ffff800;
// Active-low PB14 (+), PB13 (-); no SEL/OK held. Runtime reset uses the same
// chord, but this gate runs before settings, power control or key processing.
class KeyGate {
 unsigned stable=0;
public:
 bool sample(uint16_t pins) {
  if((pins&0xf000)!=0x9000) stable=0;
  else if(stable<40) ++stable;
  return stable==40;
 }
};
inline bool valid_vectors(uint32_t sp,uint32_t pc) {
 // ST's F103 ROM uses MSP 0x200001fc on some revisions: word aligned,
 // not necessarily 8-byte aligned. Preserve the ROM's own initial value.
 return sp>0x20000000 && sp<=0x20005000 && !(sp&3) &&
        (pc&1) && (pc&~1U)>=rom_base && (pc&~1U)<rom_end;
}
// Called only from early main, with IRQs masked. No ADC, PWM or USART has
// started. Poll is injectable for clock-readiness tests, and bounded on target.
template<class Rcc,class Tick,class Nvic,class Scb,class Flash,class Gpio,class Poll>
bool prepare(Rcc& r,Tick& tick,Nvic& nvic,Scb& scb,Flash& flash,
             Gpio& a,Gpio& b,Gpio& c,Poll poll,unsigned budget=1000000) {
 tick.CTRL=0; tick.LOAD=0; tick.VAL=0;
 for(unsigned i=0;i<2;++i) { nvic.ICER[i]=0xffffffff; nvic.ICPR[i]=0xffffffff; }
 scb.ICSR=(1U<<25)|(1U<<27); // clear pending SysTick and PendSV
 // Stop DMA clock; reset the peripherals touched by startup.
 r.AHBENR=0x14;
 r.APB1RSTR=7; r.APB1RSTR=0; // TIM2/3/4
 r.APB2RSTR=0x4e3d; r.APB2RSTR=0; // AFIO, GPIOA..D, ADC1/2, TIM1, USART1
 r.APB1ENR=0; r.APB2ENR=0x1d; // only GPIOA/B/C and AFIO remain accessible
 // Keep PA7 test signal, PB8 trigger and PB9 inverter driven LOW. Keep LCD
 // strobes/reset HIGH so the update screen remains latched during flashing.
 a.ODR=0; a.CRL=0x34444444; a.CRH=0x44444444;
 b.ODR=0x0c00; b.CRL=0x44444444; b.CRH=0x44443333;
 c.ODR=0xe000; c.CRL=0x44444444; c.CRH=0x33344444;
 r.CR|=1;
 unsigned remaining=budget;
 while(!(r.CR&2) && remaining--) poll();
 if(!(r.CR&2)) return false;
 r.CFGR&=~3U; remaining=budget;
 while((r.CFGR&12) && remaining--) poll();
 if(r.CFGR&12) return false;
 r.CR=0x83; r.CFGR=0; r.CIR=0x009f0000;
 flash.ACR=0;
 scb.VTOR=rom_base;
 return true;
}
}
