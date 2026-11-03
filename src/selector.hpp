// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace dso {
constexpr uint8_t sensitivity_code(uint8_t sen1,uint8_t sen2,uint8_t minimum=4) {
 return uint8_t(minimum+3*sen1+sen2);
}
constexpr uint8_t coupling_code(uint8_t position) { return uint8_t(2-position); }
class Selector {
 uint8_t position_=255,candidate_=255,count_=0;
public:
 uint8_t update(uint16_t raw) {
  const int low=0x300,high=0x900,h=64;
  uint8_t next;
  if(position_==0) next=raw>high+h?2:raw>low+h?1:0;
  else if(position_==1) next=raw<low-h?0:raw>high+h?2:1;
  else if(position_==2) next=raw<low-h?0:raw<high-h?1:2;
  else next=raw>high?2:raw>low?1:0;
  if(next!=candidate_) { candidate_=next; count_=1; }
  else if(count_<3) ++count_;
  if(count_==3) position_=candidate_;
  return position_;
 }
};
}
