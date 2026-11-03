// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace board {
#ifdef DSO_LEGACY_ENGINE
constexpr uint32_t core_hz=72000000,adc_divider=6,pll_multiplier=9;
constexpr uint32_t timer4_period=4096;
#else
constexpr uint32_t core_hz=56000000,adc_divider=4,pll_multiplier=7;
// Preserve the inverter carrier near 72 MHz/4096 = 17.578 kHz.
constexpr uint32_t timer4_period=3186;
#endif
constexpr uint32_t adc_hz=core_hz/adc_divider;
constexpr uint32_t systick_reload=core_hz/1000-1;
constexpr uint32_t continuous_sample_ticks=14*adc_divider;
constexpr uint32_t adc_prescaler_bits=adc_divider==4?1:2;
constexpr uint32_t clock_cfgr=(4U<<8)|(adc_prescaler_bits<<14)|(1U<<16)|((pll_multiplier-2)<<18);
constexpr uint32_t timer4_compare(uint32_t logical_u12) {
 return logical_u12*timer4_period/4096;
}
constexpr uint32_t timer4_duty_hundredths(uint32_t logical_u12) {
 return (timer4_compare(logical_u12)*10000+timer4_period/2)/timer4_period;
}
static_assert(adc_hz<=14000000,"ADC clock must remain within STM32F103 rating");
static_assert(core_hz==8000000*pll_multiplier,"Board HSE is 8 MHz");
template<class TickRegisters> void init_systick(TickRegisters& timer) {
 timer.CTRL=0; timer.LOAD=systick_reload; timer.VAL=0; timer.CTRL=7;
}
}

