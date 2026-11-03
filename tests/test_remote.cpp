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
U16 VirtAddVarTab[NumbOfVar],settings[NumbOfVar]{}; unsigned writes=0; bool flash_fail=false;
U16 EE_ReadVariable(U16 address,U16* value) { *value=settings[address]; return 0; }
U16 EE_WriteVariable(U16 address,U16 value) { if(flash_fail) return 0x80; settings[address]=value; ++writes; return FLASH_COMPLETE; }
void HostDSOInit() {}
void OutputTLvl() { TIM4->CCR3=board::timer4_compare(2048+GetVPosOfs()+GetTrigLvl()); }
extern "C" int PowerReadSwitches(U16*,U16*,U16*) { return 0; }
unsigned checks=0;
void check(bool ok,const char* name) { if(!ok) { printf("FAIL %s\n",name); exit(1); } ++checks; printf("PASS %s\n",name); }
dso::ControlState command(const char* text) { dso::ControlState s; DsoControl(text,&s); return s; }
int main() {
 for(unsigned i=0;i<NumbOfVar;++i) VirtAddVarTab[i]=i;
 settings[Addr_SettingStatus]=SettingStatus_Initialized; SetVPosOfs(23);settings[Addr_VPosOfs]=23; SetVSen(VS_1V);SetCpl(CP_AC);
 check(command("GET").zero==23,"GET reports actual calibrated state");
 check(!command("TB 24").result && GetTimebase()==24 && active && continuous_mode,"remote fastest timebase restarts nominal 1 MS/s acquisition");
 command("HOLD 1"); check(!active && (status&DSO_Hold),"remote HOLD stops hardware");
 command("TB 18"); check(!active && (status&DSO_Hold) && GetTimebase()==18,"timebase change preserves HOLD");
 command("MODE 2"); command("ARM"); check(active && !(status&DSO_Hold) && GetTrigMode()==TM_Single,"ARM rearms SINGLE from HOLD");
 command("EDGE 1");check(rising,"remote slope changes actual trigger direction");
 command("TL 1500");check(threshold==3571,"remote trigger includes calibrated zero");
 check(command("TL 4095").result==2 && threshold==3571,"invalid calibrated trigger rejected without clamping");
 command("VP -50");check(GetVPos()==-50,"remote vertical position applied");
 command("HP -100");check(GetHPos()==0,"normalized left HP endpoint");
 command("HP 100");check(GetHPos()==724,"normalized right HP endpoint");
 command("HP 0");check(GetHPos()==362,"normalized HP center");
 command("GEN 12345");command("GENON 0");check(generator_hz==12345 && !generator_enabled,"generator frequency and disable applied independently");
 command("GENON 1");check(generator_enabled,"generator enabled without changing target");
 command("FOCUS 8");command("CA 100");command("CB 200");check(focus==FC_Cursors && cursor_x[0]==100 && cursor_x[1]==200,"remote time cursor positions");
 command("CMODE 1");command("CA 30");check(cursor_voltage && cursor_y[0]==30,"remote voltage cursor mode");
 check(command("CB 300").result==2 && cursor_y[1]==167,"voltage cursor bounds enforced");
 const unsigned before=writes;command("VP 10");command("TL 0");check(writes==before,"direct remote changes do not wear settings flash");
 command("SAVE");check(writes>before && settings[Addr_Vpos]==10,"explicit SAVE persists settings");
 const unsigned saved=writes;command("SAVE");check(writes==saved,"unchanged SAVE avoids redundant flash writes");
 command("FOCUS 9");check(focus==FC_Debug,"remote Debug focus accepted and reported");
 const char* invalid[]={"TB 25","TB -1","TB 18junk","TB 100000000000000000","MODE 3","GEN 0","GENON 2","VP 256","HP 101","FOCUS 10","KEY 5","CMODE 2","RESET","RESET ALL","GET junk"};
 bool rejected=true; for(const char* p:invalid) rejected&=command(p).result==2;
 check(rejected,"malformed and out-of-range commands rejected");
 check(command("VS 4").result==1 && command("CPL 2").result==1,"mechanical selectors cannot be overridden remotely");
 check(command("RESET SETTINGS").result==0 && GetTimebase()==TB_1ms && trigmode==TM_Auto && edge==TE_Falling && vpos==0 && hpos==362 && reclen==1024,"factory reset restores acquisition defaults");
 check(voffset==23 && settings[Addr_VPosOfs]==23 && vsen==VS_1V && coupling==CP_AC,"factory reset retains calibration and physical selectors");
 check(generator_hz==1000 && generator_enabled && !cursor_voltage && measurement_panel && focus==FC_Timebase && active,"factory reset restores UI generator and RUN");
 settings[Addr_Vpos]=99; flash_fail=true; check(command("RESET SETTINGS").result==4,"flash failure reported to PC");flash_fail=false;
 SetVPos(42);Keypad.KScanBuf=0xffff;button_port=0x9fff;SystemMilliseconds=100;KeyScan();SystemMilliseconds=2099;KeyScan();
 check(vpos==42,"reset chord waits two full seconds");SystemMilliseconds=2100;KeyScan();check(vpos==0,"held plus-minus chord executes reset");
 SetVPos(17);SystemMilliseconds=7000;KeyScan();check(vpos==17,"reset chord executes once until release");button_port=0xffff;KeyScan();
 dso::FrameExporter tx;tx.set_control_handler(DsoControl);
 for(const char* p="@65535 GET\n";*p;++p) tx.receive(*p);
 check(tx.prepare_control(1234) && tx.size()==84 && tx.data()[5]==4 && tx.data()[50]==255 && tx.data()[51]==255,"request ID and typed control reply round trip");
 check(dso::crc32(tx.data(),80)==(uint32_t(tx.data()[80])|(uint32_t(tx.data()[81])<<8)|(uint32_t(tx.data()[82])<<16)|(uint32_t(tx.data()[83])<<24)),"control reply CRC matches wire contents");
 FILE* f=fopen("analysis/control-fixture.bin","wb");fwrite(tx.data(),1,tx.size(),f);fclose(f);
 for(const char* p="@65536 VP 55\n@0 VP 55\n@1x VP 55\n";*p;++p) tx.receive(*p);
 check(!tx.prepare_control(1300) && vpos==17,"invalid request identifiers cannot execute a command");
 for(const char* p="@2 VP 20\n@3 VP 30\n";*p;++p) tx.receive(*p);
 check(vpos==20 && tx.prepare_control(1400),"pending reply prevents queued command overwrite");
 printf("%u checks passed (remote control and factory reset).\n",checks);
}
