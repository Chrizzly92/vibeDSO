// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "clock_profile.hpp"
namespace board {
constexpr uint32_t test_timer_hz=core_hz;
constexpr uint16_t test_psc=core_hz/10000-1,test_arr=9,test_compare=5;
static_assert(test_timer_hz/(test_psc+1)/(test_arr+1)==1000,"test output must be 1 kHz");
static_assert(test_compare*2==test_arr+1,"test output must have 50 percent duty");
template<class Timer> void init_test_signal(Timer& t) {
 t.CR1=0; t.CR2=0; t.SMCR=0; t.DIER=0; t.SR=0;
 // CH2 PWM1 + preload. Toggle mode (3) would halve the output frequency.
 t.CCMR1=(6U<<12)|(1U<<11); t.CCMR2=0;
 t.CCER=1U<<4; // CH2 enabled, active high, other channels disabled.
 t.CNT=0; t.PSC=test_psc; t.ARR=test_arr;
 t.CCR1=0; t.CCR2=test_compare; t.CCR3=0; t.CCR4=0;
 t.DCR=0; t.DMAR=0;
 t.EGR=1; // UG loads the prescaler and shadow registers before CEN.
 t.SR=0; t.CR1=(1U<<7)|1U;
}
struct GeneratorTiming { uint16_t psc,arr,compare; uint64_t frequency_uhz; };
constexpr GeneratorTiming generator_timing(uint32_t hz) {
 hz=hz<1?1:hz>100000?100000:hz;
 const uint64_t divider=(uint64_t(core_hz)+uint64_t(hz)*65536-1)/(uint64_t(hz)*65536);
 const uint64_t period=(uint64_t(core_hz)+uint64_t(hz)*divider/2)/(uint64_t(hz)*divider);
 return {uint16_t(divider-1),uint16_t(period-1),uint16_t((period+1)/2),
         uint64_t(core_hz)*1000000/(divider*period)};
}
template<class Timer> void configure_generator(Timer& t,uint32_t hz,bool enabled) {
 const auto c=generator_timing(hz);
 t.CR1=0; t.CCER=0; t.DIER=0; t.SR=0;
 t.CNT=0; t.PSC=c.psc; t.ARR=c.arr; t.CCR2=c.compare;
 t.CCMR1=(6U<<12)|(1U<<11); t.EGR=1; t.SR=0;
 t.CCER=enabled?(1U<<4):0;
 // Disabled PWM remains low (CCR2=0) with channel active, avoiding a floating pin.
 if(!enabled) { t.CCR2=0; t.EGR=1; t.SR=0; t.CCER=1U<<4; }
 t.CR1=(1U<<7)|1U;
}
constexpr uint32_t generator_frequencies[]={1,2,5,10,20,50,100,200,500,1000,2000,5000,10000,20000,50000,100000};

}
