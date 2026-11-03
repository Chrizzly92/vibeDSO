// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace dso {
// 10-ms scanner: each press starts at one; held distance crosses decades.
class ButtonRepeat {
 uint16_t physical=0xffff,step_=1;
 unsigned stable=0,repeat=0;
 uint32_t distance=0;
 bool held=false;
 uint16_t emit(bool long_press) {
  step_=distance<10?1:distance<100?10:distance<1000?100:1000;
  if(distance<1000000) distance+=step_;
  return long_press?physical&0xfffe:physical;
 }
public:
 uint16_t step() const { return step_; }
 uint16_t update(uint16_t scan,bool slow=false) {
  const bool short_ok=physical==0x7fff && scan==0xffff && stable>=2 && !held;
  if(scan!=physical) { physical=scan; stable=repeat=distance=0; step_=1; held=false; }
  else if(stable<60) ++stable;
  if(short_ok) return 0x7fff;
  if(scan==0xffff) return 0xffff;
  const bool adjustable=scan==0xbfff || scan==0xdfff;
  if(!held) {
   if(stable==2 && scan!=0x7fff) return emit(false);
   if(stable==(adjustable?30U:60U)) { held=true; repeat=0; return emit(true); }
  } else if(adjustable && ++repeat>=(slow?4U:2U)) { repeat=0; return emit(true); }
  return 0xffff;
 }
};
}
