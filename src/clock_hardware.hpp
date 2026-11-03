// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "clock_profile.hpp"
namespace board {
// SystemInit may already have selected PLL. Switch to HSI and stop PLL before
// changing PLLMULL; writing the multiplier while PLL is running is not valid.
template<class Rcc,class Flash,class Poll>
bool configure_clock(Rcc& r,Flash& flash,Poll poll,unsigned budget=1000000) {
 auto wait=[&](auto ready) {
  for(unsigned remaining=budget;remaining;--remaining) {
   if(ready()) return true;
   poll();
  }
  return ready();
 };
 r.CR|=1U;
 if(!wait([&]{return (r.CR&2U)!=0;})) return false;
 r.CFGR&=~3U;
 if(!wait([&]{return (r.CFGR&12U)==0;})) return false;
 r.CR&=~(1U<<24);
 if(!wait([&]{return (r.CR&(1U<<25))==0;})) return false;
 r.CR|=1U<<16;
 if(!wait([&]{return (r.CR&(1U<<17))!=0;})) return false;
 flash.ACR=(flash.ACR&~7U)|2U|(1U<<4); // >=48 MHz: two wait states, prefetch.
 r.CFGR=clock_cfgr;
 r.CR|=1U<<24;
 if(!wait([&]{return (r.CR&(1U<<25))!=0;})) return false;
 r.CFGR=(r.CFGR&~3U)|2U;
 return wait([&]{return (r.CFGR&12U)==8U;});
}
}
