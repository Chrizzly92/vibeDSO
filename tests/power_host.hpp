// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "Common.h"
#include "Screen.h"
struct PowerRegisters { uint32_t CRL=0xffffffff,SMPR2=0,JSQR=0,CR1=0,CR2=0,SR=0,JDR1=0,JDR2=0,JDR3=0,JDR4=0,CCR3=2048,CCR4=1023,CCMR2=0,SQR3=3; };
inline PowerRegisters power_adc2,power_timer4,power_gpioa;
#define ADC2 (&power_adc2)
#define TIM4 (&power_timer4)
#define GPIOA (&power_gpioa)
constexpr uint32_t ADC_CR2_JEXTSEL=0x7000,ADC_CR2_JEXTTRIG=0x8000,ADC_CR2_JSWSTART=0x200000,ADC_SR_JEOC=4,TIM_CCMR2_OC4PE=0x800;
constexpr uint32_t ADC_CR1_SCAN=0x100;
inline void InjectedScan(const uint16_t (&channels)[7]) {
 const unsigned count=(ADC2->CR1&ADC_CR1_SCAN)?((ADC2->JSQR>>20)&3)+1:1;
 uint32_t* results[]={&ADC2->JDR1,&ADC2->JDR2,&ADC2->JDR3,&ADC2->JDR4};
 for(unsigned rank=0;rank<count;++rank) {
  const unsigned channel=(ADC2->JSQR>>(rank*5))&31;
  if(channel<7) *results[rank]=channels[channel];
 }
 ADC2->SR|=ADC_SR_JEOC;
}
inline volatile uint32_t SystemMilliseconds=0;
inline FONT ASC8X16{};
inline char power_text[8]{};
inline void PutcGenic(U16 x,U16 y,U8 ch,U16,U16,FONT*) {
 if(y==0 && x>=256 && x<312) power_text[(x-256)/8]=char(ch);
}
