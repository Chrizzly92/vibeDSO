// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "stm32f10x_it.h"
#include "Board.h"
#include "Command.h"
#include "libdso138.h"
#include "power.hpp"
void NMI_Handler(void) {}
void HardFault_Handler(void) { for(;;) {} }
void MemManage_Handler(void) { for(;;) {} }
void BusFault_Handler(void) { for(;;) {} }
void UsageFault_Handler(void) { for(;;) {} }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
volatile uint32_t SystemMilliseconds=0;
void SysTick_Handler(void) {
 ++SystemMilliseconds;
 PowerTick();
#ifdef DSO_SOURCE_ENGINE
 DsoTick();
#endif
 if(--TimerKeyScan==0) {
  TimerKeyScan=10;
  BitSet(Keypad.Flags,1<<KF_DoKeyScan);
 }
 if(GTimer && --GTimer==0) GTimeout=1;
}
