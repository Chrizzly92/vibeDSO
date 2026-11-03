// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#ifdef DSO_SOURCE_ENGINE
#include "stm32f10x.h"
#include "Board.h"
#include "Screen.h"
#include "clock_profile.hpp"
#include "bootloader.hpp"
#include "bootloader_core.hpp"

// No compiler-generated stack access is permitted after replacing MSP.
extern "C" __attribute__((naked,noreturn,noinline))
void BootloaderTailJump(uint32_t,uint32_t) {
 __asm volatile(
  "movs r2, #0\n"
  "msr control, r2\n"
  "msr basepri, r2\n"
  "msr faultmask, r2\n"
  "msr msp, r0\n"
  "dsb\n"
  "isb\n"
  "cpsie i\n"
  "bx r1\n");
}
namespace {
void message(const char* first,const char* second) {
 FillRect(0,136,320,64,clBlack);
 PutsGenic(40,144,(U8*)first,clWhite,clBlack,&ASC8X16);
 PutsGenic(40,168,(U8*)second,clYellow,clBlack,&ASC8X16);
}
void hex_word(int x,int y,const char* prefix,uint32_t value) {
 char label[12]; label[0]=prefix[0];label[1]=prefix[1];label[2]=' ';
 for(unsigned i=0;i<8;++i) { const unsigned digit=(value>>(28-4*i))&15;label[3+i]=char(digit<10?'0'+digit:'A'+digit-10); }
 label[11]=0;
 PutsGenic(x,y,(U8*)label,clYellow,clBlack,&ASC8X16);
}
[[noreturn]] void fail(const char* reason,uint32_t sp,uint32_t pc) {
 // prepare() may already have reset the LCD data GPIO before timing out.
 GPIOB->CRL=0x33333333;
 message("Bootloader error",reason);
 hex_word(40,184,"SP",sp);hex_word(176,184,"PC",pc);
 for(;;) __asm volatile("nop");
}
[[noreturn]] void enter() {
 const auto* vectors=reinterpret_cast<const volatile uint32_t*>(boot::rom_base);
 const uint32_t sp=vectors[0],pc=vectors[1];
 if(!boot::valid_vectors(sp,pc)) fail("ROM vectors invalid",sp,pc);
 message("Firmware update","UART1 / CH340 ready");
 __asm volatile("cpsid i":::"memory");
 if(!boot::prepare(*RCC,*SysTick,*NVIC,*SCB,*FLASH,*GPIOA,*GPIOB,*GPIOC,
                  [] { __asm volatile("nop"); })) fail("Clock handoff failed",sp,pc);
 __asm volatile("dsb\nisb":::"memory");
 BootloaderTailJump(sp,pc);
}
}
void BootloaderHoldOutputsOff() {
 GPIOA->BRR=1U<<7; GPIOB->BRR=1U<<9;
 GPIOA->CRL=(GPIOA->CRL&~(15U<<28))|(3U<<28);
 GPIOB->CRH=(GPIOB->CRH&~(15U<<4))|(3U<<4);
}
void BootloaderStartupWindow() {
 // Poll the core timer: no SysTick IRQ or application services yet.
 SysTick->CTRL=0; SysTick->LOAD=board::systick_reload; SysTick->VAL=0;
 SysTick->CTRL=5; // core clock, enable; TICKINT stays off
 boot::KeyGate gate;
 for(unsigned ms=0;ms<500;++ms) {
  while(!(SysTick->CTRL&(1U<<16))) {}
  if(gate.sample(uint16_t(GPIOB->IDR))) { SysTick->CTRL=0; enter(); }
 }
 SysTick->CTRL=0; SysTick->LOAD=0; SysTick->VAL=0;
 // Normal startup will now configure TIM3/TIM4; restore their AF outputs.
 GPIOA->CRL=(GPIOA->CRL&~(15U<<28))|(11U<<28);
 GPIOB->CRH=(GPIOB->CRH&~(15U<<4))|(11U<<4);
}
#endif
