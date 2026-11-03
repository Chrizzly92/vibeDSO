// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#define DSO_HOST_TEST
#define DSO_COMMAND_HOST_TEST
#define DSO_SOURCE_ENGINE
#include "../src/engine.cpp"
#define DSO_Init HostDSOInit
#include "../src/Command.cpp"
#undef DSO_Init
#include <stdio.h>
#include <stdlib.h>

U16 VirtAddVarTab[NumbOfVar];
U16 settings[NumbOfVar]{};
unsigned writes=0;
U16 EE_ReadVariable(U16 address,U16* value) { *value=settings[address]; return 0; }
U16 EE_WriteVariable(U16 address,U16 value) { settings[address]=value; ++writes; return FLASH_COMPLETE; }
void HostDSOInit() {} // Peripheral startup is outside the command tests.
void OutputTLvl() { TIM4->CCR3=board::timer4_compare(2048+GetVPosOfs()+GetTrigLvl()); }
extern "C" int PowerReadSwitches(U16*,U16*,U16*) { return 0; }
unsigned checks=0;
void check(bool good,const char* name) {
 if(!good) { printf("FAIL %s\n",name); exit(1); }
 ++checks; printf("PASS %s\n",name);
}
void scan(U16 physical) {
 button_port=physical; Keypad.KeyCode=0; KeyScan();
 if(Keypad.KeyCode) { KeyProc(); Keypad.KeyCode=0; }
}
int main() {
 for(unsigned i=0;i<NumbOfVar;++i) VirtAddVarTab[i]=i;
 settings[Addr_SettingStatus]=SettingStatus_Initialized;
 settings[Addr_Vpos]=static_cast<U16>(-70);
 settings[Addr_VPosOfs]=180;
 settings[Addr_Vsen]=VS_1V; settings[Addr_TimeBase]=TB_1ms;
 settings[Addr_TrigLvl]=100; settings[Addr_RecLen]=1024; settings[Addr_HPos]=362;
 AppInit();
 check(GetVPos()==0 && ycoord(2048+180)==117,"actual AppInit centers calibrated zero despite saved displaced VPOS");
 check(GetVPosOfs()==180 && GetTrigLvl()==100 && threshold==2328,"boot keeps zero calibration and calibrated trigger threshold");
 check(writes==0,"boot centering does not rewrite EEPROM");
 SetVPos(55); AppInit();
 check(GetVPos()==0 && GetVPosOfs()==180,"repeated boot recenters position while retaining calibration");
 SetFocus(FC_VPos); SetVPos(0);

 unsigned held_writes=writes; bool seen_one=false,seen_ten=false,seen_hundred=false;
 for(int tick=0;tick<=180;++tick) {
  const int before=GetVPos(); scan(0xbfff); const int delta=GetVPos()-before;
  seen_one |= delta==1; seen_ten |= delta==10; seen_hundred |= delta==100;
 }
 check(seen_one && seen_ten && seen_hundred && GetVPos()==255,"held vertical plus crosses decade steps and respects the endpoint");
 check(writes-held_writes==1,"held repetition avoids EEPROM writes after the initial press");
 scan(0xffff); const int released=GetVPos();
 for(int tick=0;tick<100;++tick) scan(0xffff);
 check(GetVPos()==released,"release stops repetition immediately");
 SetVPos(0); seen_one=seen_ten=seen_hundred=false;
 for(int tick=0;tick<=180;++tick) {
  const int before=GetVPos(); scan(0xdfff); const int delta=GetVPos()-before;
  seen_one |= delta==-1; seen_ten |= delta==-10; seen_hundred |= delta==-100;
 }
 check(seen_one && seen_ten && seen_hundred && GetVPos()==-255,"held vertical minus accelerates symmetrically through decade steps");
 scan(0xffff);
 bool all_ranges=true;
 for(int sen=4;sen<=12;++sen) {
  SetVSen(sen); SetVPosOfs(23); SetVPos(-40); SetFocus(FC_TrigLvl); SetTrigLvl(-200);
  for(int step=0;step<250;++step) {
   const int old=raw_ycoord(threshold); DoKeyIncH();
   if(raw_ycoord(threshold)!=old-1) all_ranges=false;
  }
  for(int step=0;step<250;++step) {
   const int old=raw_ycoord(threshold); DoKeyDecH();
   if(raw_ycoord(threshold)!=old+1) all_ranges=false;
  }
 }
 check(all_ranges,"actual held trigger handlers move exactly one displayed pixel in both directions across all nine gains");
 SetTrigLvl(-100); const int old=raw_ycoord(threshold); DoKeyInc();
 check(raw_ycoord(threshold)==old-1,"short trigger press also moves exactly one pixel");
 SetTrigLvl(32767); const int top=threshold; DoKeyIncH();
 check(threshold==4095 && top==4095,"one-pixel trigger stepping stops safely at ADC upper limit");
 SetTrigLvl(-32768); DoKeyDecH();
 check(threshold==0,"one-pixel trigger stepping stops safely at ADC lower limit");
 SetFocus(FC_VPos); SetVPos(255); DoKeyIncH(); SetVPos(-255); DoKeyDecH();
 check(GetVPos()==-255,"held vertical movement respects both viewport position limits");
 SetVPos(-25); SetVPosOfs(0); average=2228; DoKeyOkH();
 check(GetVPosOfs()==180 && settings[Addr_VPosOfs]==180 && ycoord(2228)==142,"long OK saves calibration and aligns resting trace with zero arrow independently of VPOS");
 scan(0xffff); SetVPosOfs(0); average=2200; const unsigned before_ok=writes;
 for(int tick=0;tick<300;++tick) scan(0x7fff);
 check(writes-before_ok==1 && GetVPosOfs()==152,"long OK calibration executes once per hold instead of repeating");
 scan(0xffff); SetFocus(FC_VPos); SetVPos(0);
 for(int i=0;i<20;++i) { scan(0xbfff); scan(0xffff); }
 check(GetVPos()==0,"contact bounce does not generate movement or long-press events");
 const bool panel_before=measurement_panel;
 DoKeySelH();
 check(measurement_panel==panel_before,"long SEL no longer toggles ADC diagnostics");
 SetFocus(FC_Debug); const U16 debug_status=GetDsoStatus(); DoKeyOk();
 check(measurement_panel!=panel_before && GetDsoStatus()==debug_status,"short OK changes Debug page without toggling HOLD");
 DoKeyOkH(); check(measurement_panel!=panel_before && GetDsoStatus()==debug_status,"long OK in Debug does not toggle page or HOLD");
 DoKeyOk();

 SetFocus(FC_TrigEdge); DoKeySel();
 check(GetFocus()==FC_Generator,"SEL reaches generator after the edge symbol");
 generator_hz=1000; generator_enabled=true; DoKeyInc();
 check(generator_hz==1001 && tim3.ARR==55943,"actual plus handler changes generator frequency");
 DoKeyDec(); DoKeyIncH(); DoKeyDecH();
 check(generator_hz==1000,"short and repeated minus/plus handlers share frequency presets");
 const auto capture_before=adc1.CR2; DoKeyOkH();
 check(!generator_enabled && tim3.CCR2==0 && adc1.CR2==capture_before,"long OK toggles generator independently of acquisition");
 DoKeySel(); check(GetFocus()==FC_Measurements,"SEL follows generator to right trigger arrow");

 scan(0xffff); SetFocus(FC_Generator); generator_hz=1000;

 bool s1=false,s10=false,s100=false,s1000=false,legal=true;
 for(unsigned tick=0;tick<140;++tick) {
  const auto prior=generator_hz; scan(0xbfff); const auto delta=generator_hz-prior;
  s1 |= delta==1; s10 |= delta==10; s100 |= delta==100; s1000 |= delta==1000;
  legal &= delta==0 || delta==1 || delta==10 || delta==100 || delta==1000;
 }
 check(legal && s1 && s10 && s100 && s1000,"held generator advances through 1 10 100 and 1000 Hz steps");
 scan(0xffff); const auto released_hz=generator_hz;
 for(int tick=0;tick<20;++tick) scan(0xdfff);
 check(generator_hz==released_hz-1,"release resets generator acceleration and a new minus press starts at one Hz");
 scan(0xffff); SetFocus(FC_Generator); DoKeySel();
 check(GetFocus()==FC_Measurements,"generator context closes through the clockwise SEL cycle");

 const U8 expected_order[]={FC_Timebase,FC_TrigMode,FC_TrigEdge,FC_Generator,FC_Measurements,FC_Cursors,FC_Debug,FC_TrigLvl,FC_HPos,FC_VPos,FC_Timebase};
 bool clockwise=true; SetFocus(expected_order[0]);
 for(unsigned i=1;i<sizeof(expected_order);++i) { DoKeySel(); clockwise &= GetFocus()==expected_order[i]; }
 check(clockwise,"SEL walks header left-to-right then right bottom left arrows before wrapping");
 scan(0xffff); SetFocus(FC_Timebase); SetTimeBase(TB_500s);
 for(unsigned tick=0;tick<50;++tick) scan(0xbfff);
 check(GetTimebase()>TB_500s,"held timebase key now scrolls sequentially through timebase options");
 scan(0xffff);

 dso::ButtonRepeat sequence; unsigned distance=0; bool decades=true;
 for(int tick=0;tick<140;++tick) {
  const U16 event=sequence.update(0xbfff);
  if(event!=0xffff) {
   const unsigned expected=distance<10?1:distance<100?10:distance<1000?100:1000;
   decades &= sequence.step()==expected; distance+=sequence.step();
  }
 }
 sequence.update(0xffff); sequence.update(0xbfff); sequence.update(0xbfff); const U16 restarted=sequence.update(0xbfff);
 check(decades && distance>1000 && restarted==0xbfff && sequence.step()==1,
       "repeat decade thresholds count moved units and reset to a one-unit short press after release");

 scan(0xffff); SetFocus(FC_Generator); generator_enabled=true; const auto nohold=status&DSO_Hold;
 for(int tick=0;tick<80;++tick) scan(0x7fff);
 check(!generator_enabled && (status&DSO_Hold)==nohold,"long OK generator action executes once without initial HOLD toggle");
 scan(0xffff); check(!generator_enabled && (status&DSO_Hold)==nohold,"release after long OK does not execute a second short action");
 for(int tick=0;tick<10;++tick) scan(0x7fff); scan(0xffff);
 check(generator_enabled && (status&DSO_Hold)==nohold,"short OK in generator menu toggles output instead of capture HOLD");
 SetFocus(FC_Timebase); status&=~DSO_Hold;
 for(int tick=0;tick<80;++tick) scan(0x7fff); scan(0xffff);
 check(!(status&DSO_Hold),"holding OK in ordinary menu cannot toggle HOLD before long-press recognition");
 for(int tick=0;tick<10;++tick) scan(0x7fff); scan(0xffff);
 check(status&DSO_Hold,"ordinary short OK toggles HOLD only after release");
 dso::ButtonRepeat fast_repeat,slow_repeat; unsigned fast_events=0,slow_events=0;
 for(int tick=0;tick<231;++tick) { fast_events+=fast_repeat.update(0xbfff)!=0xffff; slow_events+=slow_repeat.update(0xbfff,true)!=0xffff; }
 check(fast_events-2==2*(slow_events-2),"position/trigger held repeat interval is exactly twice the ordinary interval");

 scan(0xffff); status=DSO_Hold; SetFocus(FC_Cursors); cursor_selected=0; cursor_voltage=false; const int old_cursor=cursor_x[0];
 DoKeyInc(); check(cursor_x[0]==old_cursor+1,"physical cursor plus handler moves the selected time cursor");
 DoKeyOk(); check(cursor_selected==1 && (status&DSO_Hold),"cursor short OK selects the other cursor without toggling HOLD");
 DoKeyOkH(); check(cursor_voltage && (status&DSO_Hold),"cursor long OK switches time/voltage mode without toggling HOLD");
 SetFocus(FC_TrigLvl); measured.minimum=1900; measured.maximum=2300; measured.valid=true; measured.clipped=false; have_measurement_frame=true; rolling=false; average=2000; SetVPosOfs(23); DoKeyOkH();
 check(threshold==2100,"actual trigger-center handler applies extrema midpoint with calibrated zero offset");
 printf("%u checks passed (actual Command.cpp and engine.cpp; EEPROM/register stubs).\n",checks);
}
