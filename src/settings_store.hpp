// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace settings {
// Two 1-KiB pages: status, reserved halfword, then {value,key} records.
// Key last: a torn value write never commits a record. Backend uses byte offsets.
template<class Flash> class Store {
 Flash& flash;
 static constexpr uint16_t erased=0xffff, receiving=0xeeee, valid=0;
 static constexpr unsigned page_size=1024, keys=20;
 static constexpr uint16_t ok=4, invalid=0xab, full=0x80;
 uint16_t header(unsigned p) const { return flash.read(p*page_size); }
 bool blank(unsigned p) const {
  for(unsigned i=0;i<page_size;i+=2) if(flash.read(p*page_size+i)!=erased) return false;
  return true;
 }
 bool lookup(unsigned p,uint16_t key,uint16_t& value) const {
  for(unsigned i=page_size-4;i>=4;i-=4) {
   if(flash.read(p*page_size+i+2)==key) {
    value=flash.read(p*page_size+i); return true;
   }
  }
  return false;
 }
 uint16_t append(unsigned p,uint16_t key,uint16_t value) {
  for(unsigned i=4;i<page_size;i+=4) {
   unsigned a=p*page_size+i;
   if(flash.read(a)!=erased || flash.read(a+2)!=erased) continue;
   auto s=flash.program(a,value);
   return s==ok ? flash.program(a+2,key) : s;
  }
  return full;
 }
 int active() const {
  const bool a=header(0)==valid,b=header(1)==valid;
  return a==b ? -1 : a ? 0 : 1;
 }
public:
 explicit Store(Flash& f):flash(f) {}
 uint16_t init() {
  // A separate completion marker distinguishes a fully copied receiver even
  // if power fails halfway through erasing its source (including its header).
  for(unsigned p=0;p<2;++p) {
   if(header(p)==receiving && flash.read(p*page_size+2)!=erased) {
    auto s=flash.erase((1-p)*page_size);
    return s==ok ? flash.program(p*page_size,valid) : s;
   }
  }
  int a=active();
  if(a>=0) {
   // With an intact source, an interrupted transfer can be abandoned safely.
   return blank(1-a) ? ok : flash.erase((1-a)*page_size);
  }
  for(unsigned p=0;p<2;++p) {
   if(header(p)==receiving && header(1-p)!=receiving && header(1-p)!=valid) {
    // The source is erased only after every committed key has been copied.
    auto s=flash.erase((1-p)*page_size);
    return s==ok ? flash.program(p*page_size,valid) : s;
   }
  }
  if(blank(0) && blank(1)) return flash.program(0,valid);
  // Keep ambiguous or unrecognised data for diagnosis instead of formatting.
  return invalid;
 }
 uint16_t read(uint16_t key,uint16_t* value) const {
  int a=active();
  if(!value || key>=keys || a<0) return 1;
  return lookup(unsigned(a),key,*value) ? 0 : 1;
 }
 uint16_t write(uint16_t key,uint16_t value) {
  if(key>=keys) return invalid;
  int a=active();
  if(a<0) return invalid;
  uint16_t old;
  if(lookup(unsigned(a),key,old) && old==value) return ok;
  auto s=append(unsigned(a),key,value);
  if(s!=full) return s;
  unsigned next=1-unsigned(a);
  s=flash.erase(next*page_size); if(s!=ok) return s;
  s=flash.program(next*page_size,receiving); if(s!=ok) return s;
  s=append(next,key,value); if(s!=ok) return s;
  for(uint16_t k=0;k<keys;++k) {
   if(k!=key && lookup(unsigned(a),k,old)) {
    s=append(next,k,old); if(s!=ok) return s;
   }
  }
  s=flash.program(next*page_size+2,0); if(s!=ok) return s;
  // No two valid pages: complete receiver survives a reset during source erase.
  s=flash.erase(unsigned(a)*page_size); if(s!=ok) return s;
  return flash.program(next*page_size,valid);
 }
};
}
