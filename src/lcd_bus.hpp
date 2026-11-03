// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
// PB0..7 carry data; PB8..15 include unrelated control/PWM signals.
// BSRR changes only the data byte. Explicit setup/pulse/hold gaps make the
// bus independent of optimizer scheduling and avoid very short WR pulses.
inline void LcdWriteByte(U8 value) {
#ifdef DSO_LCD_HOST_TEST
 LcdHostWriteByte(value);
#else
 GPIOB->BSRR=((~uint32_t(value)&0xffU)<<16)|value;
 __asm volatile("nop\nnop\nnop" ::: "memory");
 TFT_nWR_Port->BRR=1U<<TFT_nWR_Bit;
 __asm volatile("nop\nnop\nnop" ::: "memory");
 TFT_nWR_Port->BSRR=1U<<TFT_nWR_Bit;
 __asm volatile("nop\nnop\nnop" ::: "memory");
#endif
}
