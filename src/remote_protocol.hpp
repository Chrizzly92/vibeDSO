// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace dso {
struct ControlState {
 uint8_t result=0,timebase=0,mode=0,edge=0,focus=0,sensitivity=0,coupling=0;
 uint16_t request=0,flags=0,hpos=0,reclen=0,cursor_a=0,cursor_b=0;
 int16_t vpos=0,trigger=0,zero=0;
 uint32_t generator=0;
};
using ControlHandler=void (*)(const char*,ControlState*);
// Strict bounded decimal parsing: no truncation, spaces, suffixes or overflow.
inline bool integer(const char* p,int32_t lo,int32_t hi,int32_t& out) {
 bool negative=*p=='-'; if(negative) ++p;
 if(*p<'0' || *p>'9') return false;
 int32_t value=0;
 for(;*p;++p) { if(*p<'0' || *p>'9' || value>1000000) return false; value=value*10+*p-'0'; }
 if(negative) value=-value;
 if(value<lo || value>hi) return false;
 out=value; return true;
}
}
