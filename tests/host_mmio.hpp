// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "Common.h"
#include "Screen.h"
#include "libdso138.h"
#include "register_bits.hpp"
// Plain registers are intentionally not a timing/peripheral model.
struct Registers {
 volatile uint32_t SMCR,DIER,CCMR2,CCR1,CCR4,DCR,DMAR,CR1,CR2,DR,SR,CCR,PSC,ARR,CNT,CCMR1,CCR2,CCER,EGR;
 volatile uint32_t SMPR1,SMPR2,SQR1,SQR2,SQR3,AHBENR,APB2ENR,APB1ENR;
 volatile uint32_t IFCR,ISR,CPAR,CMAR,CNDTR,CCR3,BSRR,BRR;
};
struct DmaFlagsModel {
 volatile uint32_t ISR=0;
 struct Clear {
  DmaFlagsModel* owner;
  void operator=(uint32_t mask) { owner->ISR &= ~((mask&1)?15U:mask); }
 } IFCR{this};
};
inline Registers adc1{},tim3{},tim1{},tim2{},tim4{},gpioa{},rcc{},dma_channel{};
inline DmaFlagsModel dma1{};
#define TIM3 (&tim3)
#define TIM4 (&tim4)
#define GPIOA (&gpioa)
#define ADC1 (&adc1)
#define TIM1 (&tim1)
#define TIM2 (&tim2)
#define RCC (&rcc)
#define DMA1 (&dma1)
#define DMA1_Channel1 (&dma_channel)
constexpr int DMA1_Channel1_IRQn=11, ADC1_2_IRQn=18;
inline void __enable_irq() {}
inline void __DMB() {}
inline void NVIC_SetPriority(int,int) {}
inline void NVIC_EnableIRQ(int) {}
inline void NVIC_ClearPendingIRQ(int) {}
inline void Delay(U16) {}
inline U16 framebuffer[240][320]{};
inline unsigned pixel_writes=0,column_writes=0,glyph_writes=0;
inline char glyph_at[240][320]{};
inline bool invalid_draw=false;
inline void ClrScreen() { for(auto& row:framebuffer) for(auto& pixel:row) pixel=0; }
inline void FillRect(S16 x,S16 y,S16 w,S16 h,U16 color) {
 if(x<0 || y<0 || w<=0 || h<=0 || x+w>320 || y+h>240) { invalid_draw=true; return; }
 for(int py=y;py<y+h;++py) for(int px=x;px<x+w;++px) { framebuffer[py][px]=color; ++pixel_writes; }
}
inline void WriteColumn(U16 x,U16 y,const U16* pixels,U16 count) {
 if(x>=320 || y+count>240 || !count) { invalid_draw=true; return; }
 ++column_writes;
 for(unsigned i=0;i<count;++i) { framebuffer[y+i][x]=pixels[i]; ++pixel_writes; }
}
inline void PutcGenic(U16 x,U16 y,U8 ch,U16 fg,U16 bg,FONT* font) {
 if(x+8>320 || y+16>240) invalid_draw=true;
 ++glyph_writes;
 if(x+8<=320 && y+16<=240) {
  FillRect(x,y,8,16,bg);
  if(font->Array && ch>=font->IndexOfs) {
   const U8* data=font->Array+(ch-font->IndexOfs)*16;
   for(unsigned row=0;row<16;++row) for(unsigned col=0;col<8;++col)
    framebuffer[y+row][x+col]=(row && (data[row-1]&(0x80>>col)))?fg:bg;
  }
 }
 if(x<320 && y<240) glyph_at[y][x]=char(ch);
}
inline void PutsGenic(U16,U16,U8*,U16,U16,FONT*) {}
inline FONT ASC8X16{};
inline volatile uint32_t SystemMilliseconds=0;

extern "C" inline uint16_t PowerDutyHundredths(void) { return 5000; }

inline uint16_t host_power_color=clGreen;
extern "C" inline uint32_t PowerReadDisplay(char text[7],uint16_t* color) {
 const char* p="=R-8.00"; for(unsigned i=0;i<7;++i) text[i]=p[i]; if(color) *color=host_power_color; return 1;
}
