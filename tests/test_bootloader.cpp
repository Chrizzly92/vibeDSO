// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "../src/bootloader_core.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>
struct Rcc { uint32_t CR=0x3000003,CFGR=8,AHBENR=0xffffffff,APB1RSTR=0,APB2RSTR=0,APB1ENR=0xffffffff,APB2ENR=0xffffffff,CIR=0; };
struct Tick { uint32_t CTRL=7,LOAD=123,VAL=456; };
struct Nvic { uint32_t ICER[2]{},ICPR[2]{}; };
struct Scb { uint32_t ICSR=0,VTOR=0x08000000; };
struct Flash { uint32_t ACR=0x12; };
struct Gpio { uint32_t ODR=0xffff,CRL=0xffffffff,CRH=0xffffffff; };
void check(bool ok,const char* label) { if(!ok) { printf("FAIL %s\n",label);exit(1); } printf("PASS %s\n",label); }
int main() {
 boot::KeyGate gate;bool entered=false;
 for(unsigned i=0;i<1000;++i) entered|=gate.sample(0xffff);
 check(!entered,"normal startup never requests ROM entry");
 for(uint16_t key:{uint16_t(0xbfff),uint16_t(0xdfff),uint16_t(0x7fff),uint16_t(0xefff),uint16_t(0x1fff),uint16_t(0x8fff)}) {
  boot::KeyGate single; for(unsigned i=0;i<100;++i) entered|=single.sample(key);
 }
 check(!entered,"single keys and extra held keys cannot request ROM entry");
 for(unsigned i=0;i<39;++i) entered|=gate.sample(0x9fff);
 check(!entered && gate.sample(0x9fff),"plus-minus must remain stable for forty one-ms samples");
 boot::KeyGate bounce;
 for(unsigned i=0;i<100;++i) { for(unsigned j=0;j<39;++j) entered=bounce.sample(0x9fff); entered|=bounce.sample(0xffff); }
 check(!entered,"release and bounce restart the boot chord debounce");
 check(boot::valid_vectors(0x20000200,0x1ffff021) && boot::valid_vectors(0x20005000,0x1ffff7ff),"valid C8 ROM vectors accepted at supported boundaries");
 check(boot::valid_vectors(0x200001fc,0x1ffff021),"actual F103 ROM vector with four-byte-aligned initial MSP accepted");
 check(!boot::valid_vectors(0x20000000,0x1ffff021) && !boot::valid_vectors(0x20005008,0x1ffff021) && !boot::valid_vectors(0x20000202,0x1ffff021),"invalid or non-word-aligned stack pointers rejected");
 check(!boot::valid_vectors(0x20000200,0x08000101) && !boot::valid_vectors(0x20000200,0x1ffff020) && !boot::valid_vectors(0x20000200,0x1ffff801),"non-ROM or non-Thumb entry points rejected");
 Rcc r;Tick tick;Nvic n;Scb scb;Flash flash;Gpio a,b,c;
 auto poll=[&]{r.CFGR&=~12U;};
 check(boot::prepare(r,tick,n,scb,flash,a,b,c,poll,4),"handoff preparation completes after modeled HSI switch");
 check(tick.CTRL==0 && tick.LOAD==0 && tick.VAL==0 && n.ICER[0]==0xffffffff && n.ICER[1]==0xffffffff && n.ICPR[1]==0xffffffff && scb.ICSR==((1U<<25)|(1U<<27)),"SysTick and all implemented NVIC banks disabled and pending IRQs cleared");
 check(r.CR==0x83 && r.CFGR==0 && flash.ACR==0 && r.APB1ENR==0 && r.AHBENR==0x14 && r.APB1RSTR==0 && r.APB2RSTR==0,"clock and used peripherals return to reset configuration without active DMA");
 check(!(a.ODR&(1U<<7)) && (a.CRL>>28)==3 && !(b.ODR&0x300) && (b.CRH&0xff)==0x33,"generator and inverter remain driven low during ROM update");
 check((b.ODR&0xc00)==0xc00 && (c.ODR&0xe000)==0xe000 && scb.VTOR==boot::rom_base,"LCD inactive strobes stay high and vector table points at ROM");
 Rcc missing;missing.CR=0;Scb unchanged;
 check(!boot::prepare(missing,tick,n,unchanged,flash,a,b,c,[]{},3) && unchanged.VTOR==0x08000000,"missing HSI readiness aborts safely without selecting ROM vectors");
 Rcc stuck;Scb unchanged2;
 check(!boot::prepare(stuck,tick,n,unchanged2,flash,a,b,c,[]{},3) && unchanged2.VTOR==0x08000000,"failed clock switch has bounded timeout before ROM jump");
}
