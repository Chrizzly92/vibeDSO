// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
struct LinkRegisters { uint32_t SR=0,DR=0,CR1=0,CR3=0,AHBENR=0,CCR=0,CPAR=0,CMAR=0,CNDTR=0; };
inline LinkRegisters uart,dma4,dma5,link_rcc;
struct LinkDmaFlags { uint32_t ISR=0; struct Clear { LinkDmaFlags* p; void operator=(uint32_t v) { p->ISR&=~v; } } IFCR{this}; };
inline LinkDmaFlags link_dma;
#define USART1 (&uart)
#define DMA1 (&link_dma)
#define DMA1_Channel5 (&dma5)
#define DMA1_Channel4 (&dma4)
#define RCC (&link_rcc)
constexpr int USART1_IRQn=37;
inline unsigned link_irq_priority=99; inline bool link_irq_enabled=false;
inline void NVIC_SetPriority(int,unsigned p) { link_irq_priority=p; }
inline void NVIC_EnableIRQ(int) { link_irq_enabled=true; }
inline volatile uint32_t SystemMilliseconds=0;
