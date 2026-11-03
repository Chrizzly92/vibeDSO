// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "../src/settings_store.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
struct LostPower {};
struct Flash {
 std::array<uint16_t,1024> data;
 int budget=-1,operations=0;
 bool partial=false;
 Flash() { data.fill(0xffff); }
 uint16_t read(unsigned a) const { return data.at(a/2); }
 void step() { if(budget==0) throw LostPower{}; if(budget>0)--budget; ++operations; }
 uint16_t program(unsigned a,uint16_t v) {
  step(); auto& old=data.at(a/2);
  if((old & v)!=v) return 2;
  old=v; return 4;
 }
 uint16_t erase(unsigned a) {
  step();
  if(partial && budget==0) {
   // Include a cut with the old header still valid and records already erased.
   std::fill(data.begin()+a/2+100,data.begin()+a/2+300,0xffff);
   throw LostPower{};
  }
  std::fill(data.begin()+a/2,data.begin()+a/2+512,0xffff);return 4;
 }
};
void check(bool b,const char* what) {
 if(!b) {std::printf("FAIL %s\n",what);std::exit(1);}
 std::printf("PASS %s\n",what);
}
bool value(settings::Store<Flash>& s,unsigned key,unsigned expected) {
 uint16_t v=0;return s.read(uint16_t(key),&v)==0 && v==expected;
}
int main() {
 Flash f;settings::Store<Flash> s(f);
 check(s.init()==4 && f.data[0]==0,"blank settings initialize without moving EEPROM addresses");
 uint16_t untouched=99;
 check(s.read(0,&untouched)==1 && untouched==99 && s.read(20,&untouched)==1 && s.read(0,nullptr)==1,"missing and invalid keys leave caller data intact");
 check(s.write(4,0xffff)==4 && value(s,4,0xffff),"all sixteen value bits including ffff survive");
 auto before=f.operations;
 check(s.write(4,0xffff)==4 && before==f.operations,"unchanged values cause no flash wear");
 check(s.write(20,1)!=4 && s.write(0xffff,1)!=4,"out-of-range keys are rejected");
 Flash legacy;legacy.data[512]=0;legacy.data[514]=123;legacy.data[515]=5;
 legacy.data[516]=456;legacy.data[517]=5;
 settings::Store<Flash> ls(legacy);
 check(ls.init()==4 && value(ls,5,456),"legacy page-one records read newest value with original layout");
 Flash interrupted;interrupted.data[0]=0;interrupted.data[2]=12;interrupted.data[3]=0;
 interrupted.data[4]=789; // no committed key
 settings::Store<Flash> is(interrupted);
 check(is.init()==4 && value(is,0,12) && is.write(1,42)==4 && value(is,1,42),"incomplete record skipped and never mistaken for committed data");
 Flash bad;bad.data[0]=0x1234;auto badcopy=bad.data;settings::Store<Flash> bs(bad);
 check(bs.init()!=4 && bad.data==badcopy,"unrecognised settings are preserved instead of silently formatted");
 Flash ambiguous;ambiguous.data[0]=ambiguous.data[512]=0;auto acopy=ambiguous.data;
 settings::Store<Flash> as(ambiguous);
 check(as.init()!=4 && ambiguous.data==acopy,"ambiguous two-valid-page state preserved for diagnosis");
 Flash full;settings::Store<Flash> fs(full);fs.init();
 for(unsigned i=0;i<255;++i) if(fs.write(i%20,uint16_t(i+1000))!=4) return 2;
 auto baseline=full;
 Flash probe=baseline;settings::Store<Flash> ps(probe);auto start=probe.operations;
 check(ps.write(0,9999)==4 && value(ps,0,9999),"full page transfers and commits the new value");
 int transfer_ops=probe.operations-start;
 bool retained=true;
 for(unsigned k=1;k<20;++k) {
  uint16_t v;fs.read(k,&v);retained &= value(ps,k,v);
 }
 check(retained,"page transfer preserves every other existing key");
 bool recovered=true;
 for(bool partial:{false,true}) for(int cut=0;cut<=transfer_ops;++cut) {
  Flash fault=baseline;fault.budget=cut;fault.partial=partial;
  settings::Store<Flash> attempt(fault);
  bool committed=false;
  try { committed=attempt.write(0,9999)==4; } catch(const LostPower&) {}
  fault.budget=-1;fault.partial=false;
  settings::Store<Flash> reboot(fault);
  if(reboot.init()!=4) {recovered=false;continue;}
  uint16_t old;fs.read(0,&old);
  recovered &= value(reboot,0,9999) || (!committed && value(reboot,0,old));
  for(unsigned k=1;k<20;++k) { fs.read(k,&old);recovered &= value(reboot,k,old); }
  recovered &= reboot.write(19,321)==4 && value(reboot,19,321);
 }
 check(recovered,"reset at every transfer operation including partial source erase retains committed settings");
 bool recovery_cuts=true;
 // Reset again during boot recovery of a fully copied receiver.
 for(int cut=0;cut<3;++cut) {
  Flash fault=baseline;fault.budget=transfer_ops-2;
  settings::Store<Flash> attempt(fault);
  try { attempt.write(0,9999); } catch(const LostPower&) {}
  fault.budget=cut;
  try { attempt.init(); } catch(const LostPower&) {}
  fault.budget=-1;
  recovery_cuts &= attempt.init()==4 && value(attempt,0,9999);
 }
 check(recovery_cuts,"boot recovery itself tolerates another reset");
 Flash oldtransfer;oldtransfer.data[512]=0xeeee;oldtransfer.data[514]=55;oldtransfer.data[515]=5;
 settings::Store<Flash> os(oldtransfer);
 check(os.init()==4 && value(os,5,55),"legacy receiving page after completed source erase is recoverable");
 bool many=true;
 for(unsigned i=0;i<6000;++i) {
  many &= s.write(i%20,uint16_t(i))==4;
  if(i%33==0)many &= s.init()==4;
 }
 for(unsigned k=0;k<20;++k)many &= value(s,k,5980+k);
 check(many,"many alternating page transfers preserve all twenty keys across reboots");
}
