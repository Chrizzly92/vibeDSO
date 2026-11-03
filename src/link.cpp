// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "clock_profile.hpp"
#include "link.hpp"
#ifdef DSO_SOURCE_ENGINE
#ifdef DSO_LINK_HOST_TEST
#include "link_host.hpp"
#endif
extern volatile uint32_t SystemMilliseconds;
namespace { dso::FrameExporter exporter; bool tx_busy=false;
 volatile uint8_t rx[64]; unsigned rx_read=0;
}
void LinkInit(void) {
 exporter.set_control_handler(DsoControl);
 rx_read=0; RCC->AHBENR|=1;
 DMA1_Channel4->CCR=0; DMA1_Channel5->CCR=0; DMA1->IFCR=0xff000;
 DMA1_Channel5->CPAR=uint32_t(reinterpret_cast<uintptr_t>(&USART1->DR));
 DMA1_Channel5->CMAR=uint32_t(reinterpret_cast<uintptr_t>(&rx[0]));
 DMA1_Channel5->CNDTR=64;
 DMA1_Channel5->CCR=(1U<<12)|(1U<<7)|(1U<<5)|1U; // RX: circular, byte, medium priority
 USART1->CR3|=(1U<<7)|(1U<<6); tx_busy=false;
}
void LinkPoll(void) {
 const uint32_t receive_flags=DMA1->ISR&0xf0000;
 const unsigned written=(64-DMA1_Channel5->CNDTR)&63;
 if((receive_flags&0x80000) || (receive_flags&0x60000)==0x60000 || (USART1->SR&(1U<<3))) {
  const volatile uint32_t discard=USART1->DR; (void)discard;
  exporter.discard_command(); rx_read=written;
 }
 DMA1->IFCR=receive_flags;
 for(unsigned budget=0;budget<64 && rx_read!=written;++budget) {
  const uint8_t ch=rx[rx_read]; rx_read=(rx_read+1)&63; exporter.receive(ch);
 }
 if(tx_busy) {
  if(!(DMA1->ISR&0xa000) && DMA1_Channel4->CNDTR) return;
  DMA1_Channel4->CCR=0; DMA1->IFCR=0xf000; tx_busy=false;
 }
 if(exporter.prepare_control(SystemMilliseconds)) {
  // Control acknowledgements share the frozen TX buffer and precede scenes.
 } else if(!exporter.wants(SystemMilliseconds)) return;
 else if(exporter.mirror()) {
  dso::MirrorView view;
  if(!DsoReadMirrorView(&view) || !exporter.prepare_mirror(view,SystemMilliseconds)) return;
 } else {
  dso::ExportFrame frame;
  if(!DsoReadExportFrame(&frame) || !exporter.prepare(frame,SystemMilliseconds)) return;
 }
 DMA1_Channel4->CPAR=uint32_t(reinterpret_cast<uintptr_t>(&USART1->DR));
 DMA1_Channel4->CMAR=uint32_t(reinterpret_cast<uintptr_t>(exporter.data()));
 DMA1_Channel4->CNDTR=exporter.size(); DMA1->IFCR=0xf000;
 DMA1_Channel4->CCR=(1U<<7)|(1U<<4)|1U; tx_busy=true; // TX: byte, low priority
}
#endif
