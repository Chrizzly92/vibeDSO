// Copyright (C)2014 JYE Tech Ltd., All right reserved
// Original code: vendor/JYE-LICENSE.txt.
// Modified for vibeDSO, 2026 Chrizzly92; modifications licensed Apache-2.0.
//////////////////////////////////////////////////////////////////////////////
//
// 	Filename:	Command.c
//	Version:		
//	Data:		
//
//	Author:		Liu, Zemin
//	Company:	JYE Tech
//
//-----------------------------------------------------------------------------
//
// 	Target: 		STM32F103C8 
// 	Tool chain: 	CodeSourcery G++
//
//-----------------------------------------------------------------------------
//	Required files:
//
//-----------------------------------------------------------------------------
//	Notes:
//
//
//-----------------------------------------------------------------------------
//	Revision History:
//
///////////////////////////////////////////////////////////////////////////////
#ifdef DSO_COMMAND_HOST_TEST
#include "host_commands.hpp"
#else
#include "stm32f10x.h"
#include "stm32f10x_conf.h"
#include "Board.h"
#endif

#include "Common.h"
#include "Command.h"
#include	"libdso138.h"
#include	"Screen.h"
#include 	"Eeprom.h"
#include "power.hpp"
#include "selector.hpp"
#ifdef DSO_SOURCE_ENGINE
#include "button_repeat.hpp"
#include "link.hpp"
#include <string.h>
static U16 held_step=1;
extern volatile uint32_t SystemMilliseconds;
#endif

// ===========================================================
// 	File Scope Global variables
// ===========================================================
//
const 	KeyScanCode KScanCodeTab[9] = {
//	scan code	key code		key param
	{0x7FFF, 	KC_SW1,		'1'},		// 1
	{0xBFFF, 	KC_SW2,		'2'},		// 2
	{0xDFFF, 	KC_SW3,		'3'},		// 3
	{0xEFFF, 	KC_SW4,		'4'},		// 4
	{0x7FFE, 	KC_SW1H,	'5'},		// 5
	{0xBFFE, 	KC_SW2H,	'6'},		// 6
	{0xDFFE, 	KC_SW3H,	'7'},		// 7
	{0xEFFE, 	KC_SW4H,	'8'},		// 8
	{0, 			0,			0}		
	};

KEYPAD	Keypad;
U8	VSenPrev, CplPrev;
U16	Flags;


// ===========================================================
//	Function Definitions
// ===========================================================
//
void	AppInit()
{
 U16 tmp0;

// =============================
// Note: DSO_Init() must be executed for proper capture engine running
 
 DSO_Init();

// =============================

 // Check EEPROM for valid settings
 EE_ReadVariable(Addr_SettingStatus, &tmp0);
 if(tmp0 == SettingStatus_Initialized) {
	// Load saved settings
#ifdef DSO_SOURCE_ENGINE
    SetVPos(0); // Always boot with the calibrated zero at screen center.
#else
	EE_ReadVariable(VirtAddVarTab[Addr_Vpos], &tmp0);
	SetVPos(tmp0);
#endif
	EE_ReadVariable(VirtAddVarTab[Addr_Vsen], &tmp0);
	SetVSen(tmp0);
	EE_ReadVariable(VirtAddVarTab[Addr_Cpl], &tmp0);
	SetCpl(tmp0);
	EE_ReadVariable(VirtAddVarTab[Addr_TimeBase], &tmp0);
	SetTimeBase(tmp0);
	EE_ReadVariable(VirtAddVarTab[Addr_TrigMode], &tmp0);
	SetTrigMode(tmp0);
	EE_ReadVariable(VirtAddVarTab[Addr_TrigEdge], &tmp0);
	SetTrigEdge(tmp0);
	// Restore calibration first: valid trigger limits depend on the zero offset.
	EE_ReadVariable(VirtAddVarTab[Addr_VPosOfs], &tmp0);
	SetVPosOfs(tmp0);
	EE_ReadVariable(VirtAddVarTab[Addr_TrigLvl], &tmp0);
	SetTrigLvl(tmp0);
	EE_ReadVariable(VirtAddVarTab[Addr_RecLen], &tmp0);
	SetRecLen(tmp0);
	EE_ReadVariable(VirtAddVarTab[Addr_HPos], &tmp0);
	SetHPos(tmp0);
 	}
 else {
	// Load default settings and initialize EEPROM
 	SetVPos(0);
	EE_WriteVariable(VirtAddVarTab[Addr_Vpos], 0);
 	SetVSen(VS_05V);
	EE_WriteVariable(VirtAddVarTab[Addr_Vsen], VS_05V);
 	SetCpl(CP_DC);
	EE_WriteVariable(VirtAddVarTab[Addr_Cpl], CP_DC);
 	SetTimeBase(TB_1ms);
	EE_WriteVariable(VirtAddVarTab[Addr_TimeBase], TB_1ms);
 	SetTrigMode(TM_Auto);
	EE_WriteVariable(VirtAddVarTab[Addr_TrigMode], TM_Auto);
 	SetTrigEdge(TE_Falling);
	EE_WriteVariable(VirtAddVarTab[Addr_TrigEdge], TE_Falling);
 	SetTrigLvl(0);
	EE_WriteVariable(VirtAddVarTab[Addr_TrigLvl], 0);
 	SetRecLen(SampleBufSizeMax);
	EE_WriteVariable(VirtAddVarTab[Addr_RecLen], SampleBufSizeMax);
 	SetHPos(GetRecLen()/2 - WDsize/2);
	EE_WriteVariable(VirtAddVarTab[Addr_HPos], GetRecLen()/2 - WDsize/2);
	SetVPosOfs(0);
	EE_WriteVariable(VirtAddVarTab[Addr_VPosOfs], 0);

	// Mark down EEPROM has been initialized
	EE_WriteVariable(VirtAddVarTab[Addr_SettingStatus], SettingStatus_Initialized);

 	}

 OutputTLvl();

 // Misc initialization
 TimerKeyScan = 1;

 Keypad.KDebounceVal = KD_val;
 
}

#ifdef DSO_SOURCE_ENGINE
static U16 SaveSettings(void) {
 const U16 indices[]={Addr_TimeBase,Addr_HPos,Addr_Vpos,Addr_TrigMode,Addr_TrigEdge,Addr_TrigLvl,Addr_RecLen};
 const U16 values[]={U16(GetTimebase()),U16(GetHPos()),U16(GetVPos()),U16(GetTrigMode()),U16(GetTrigEdge()),U16(GetTrigLvl()),GetRecLen()};
 U16 result=0;
 for(unsigned i=0;i<sizeof(indices)/sizeof(indices[0]);++i) {
  U16 old=0;
  if(EE_ReadVariable(VirtAddVarTab[indices[i]],&old)!=0 || old!=values[i]) {
   if(EE_WriteVariable(VirtAddVarTab[indices[i]],values[i])!=FLASH_COMPLETE) result=1;
  }
 }
 return result;
}
U16 AppFactoryReset(void) {
 StopCapture(); SetHold(); SetVPos(0); SetTimeBase(TB_1ms); SetTrigMode(TM_Auto); SetTrigEdge(TE_Falling);
 SetTrigLvl(0); SetRecLen(1024); SetHPos(362); DsoResetView();
 // Mechanical selectors, saved zero offset, gain trims and power protection stay intact.
 const U16 result=SaveSettings(); ClrHold(); StartCapture(); return result;
}
void DsoControl(const char* command,dso::ControlState* reply) {
 if(!reply) return;
 char name[12]{}; unsigned n=0;
 while(command[n] && command[n]!=' ' && n<sizeof(name)-1) { name[n]=command[n]; ++n; }
 const char* arg=command+n; const bool has_arg=*arg==' '; if(has_arg) ++arg;
 int32_t value=0; reply->result=0;
 const auto number=[&](int32_t lo,int32_t hi) { if(!has_arg || !dso::integer(arg,lo,hi,value)) { reply->result=2; return false; } return true; };
 const auto no_arg=[&]() { if(*arg) { reply->result=2; return false; } return true; };
 const auto restart=[]() { UpdateDisp(Disp_Trace|Disp_Param); if(!(GetDsoStatus()&DSO_Hold)) StartCapture(); };
 if(!strcmp(name,"GET")) { no_arg(); }
 else if(!strcmp(name,"TB")) { if(number(TBMin,TBMax)) { StopCapture(); SetTimeBase(value); restart(); } }
 else if(!strcmp(name,"MODE")) { if(number(TM_Auto,TM_Single)) { StopCapture(); SetTrigMode(value); restart(); } }
 else if(!strcmp(name,"EDGE")) { if(number(TE_Falling,TE_Rising)) { SetTrigEdge(value); restart(); } }
 else if(!strcmp(name,"TL")) { if(number(-2048-GetVPosOfs(),2047-GetVPosOfs())) SetTrigLvl(value); }
 else if(!strcmp(name,"VP")) { if(number(VPosMin,VPosMax)) SetVPos(value); }
 else if(!strcmp(name,"HP")) { if(number(-100,100)) SetHPos((value+100)*(GetRecLen()-300)/200); }
 else if(!strcmp(name,"HOLD")) { if(number(0,1)) { if(value) SetHold(); else { ClrHold(); StartCapture(); } } }
 else if(!strcmp(name,"ARM")) { if(no_arg()) { ClrHold(); StartCapture(); } }
 else if(!strcmp(name,"GEN")) { if(number(1,100000)) { dso::ControlState state; DsoReadControlState(&state); DsoSetGenerator(value,state.flags&2); } }
 else if(!strcmp(name,"GENON")) { if(number(0,1)) { dso::ControlState state; DsoReadControlState(&state); DsoSetGenerator(state.generator,value); } }
 else if(!strcmp(name,"FOCUS")) { if(number(0,9)) { SetFocus(value); UpdateDisp(Disp_Trace|Disp_Param); } }
 else if(!strcmp(name,"KEY")) { if(number(0,4)) { switch(value) { case 0: DoKeySel(); break; case 1: DoKeyInc(); break; case 2: DoKeyDec(); break; case 3: DoKeyOk(); break; case 4: DoKeyOkH(); break; } } }
 else if(!strcmp(name,"CA") || !strcmp(name,"CB")) {
  dso::ControlState state; DsoReadControlState(&state); const bool voltage=state.flags&8;
  if(number(voltage?17:10,voltage?216:309)) DsoSetCursor(voltage,name[1]=='B',value);
 }
 else if(!strcmp(name,"CMODE")) {
  if(number(0,1)) { dso::ControlState state; DsoReadControlState(&state); if(bool(state.flags&8)!=bool(value)) ToggleCursorMode(); }
 }
 else if(!strcmp(name,"SAVE")) { if(no_arg() && SaveSettings()) reply->result=4; }
 else if(!strcmp(name,"RESET")) {
  if(!has_arg || strcmp(arg,"SETTINGS")) reply->result=2;
  else if(AppFactoryReset()) reply->result=4;
 }
 else reply->result=1;
 DsoReadControlState(reply);
}
#endif

void	KeyProc(void)
{
 switch(Keypad.KeyCode) {
	case KC_SW1:
		DoKeyOk();
	default:	
		break;
		
	case KC_SW2:
		DoKeyInc();
		break;

	case KC_SW3:
		DoKeyDec();
		break;

	case KC_SW4:
		DoKeySel();
		break;

	case KC_SW1H:
		DoKeyOkH();
		break;

	case KC_SW2H:
		DoKeyIncH();
		break;

	case KC_SW3H:
		DoKeyDecH();
		break;

	case KC_SW4H:
		DoKeySelH();
		break;

 	}
}


void KeyScan(void)
{
 U16	tmp1;

 Keypad.KScanBuf = NoKey;
 // Read buttons
 tmp1 = (PB_Port & PB_Bits) | ~PB_Bits;
 if(tmp1 != NoKey) {
	Keypad.KScanBuf = tmp1;
 	}

 
 // -- Debouncing
#ifdef DSO_SOURCE_ENGINE
 static dso::ButtonRepeat buttons;
 static bool reset_down=false,reset_done=false; static uint32_t reset_started=0;
 if(Keypad.KScanBuf==0x9fff) { // PLUS + MINUS, two seconds, once until release.
  Keypad.KeyCode=0; buttons.update(NoKey);
  if(!reset_down) { reset_down=true; reset_done=false; reset_started=SystemMilliseconds; }
  if(!reset_done && uint32_t(SystemMilliseconds-reset_started)>=2000) { reset_done=true; AppFactoryReset(); }
  return;
 }
 reset_down=reset_done=false;
 const U16 event=buttons.update(Keypad.KScanBuf,GetFocus()==FC_VPos || GetFocus()==FC_TrigLvl);
 held_step=buttons.step();
 Keypad.KScanCode=Keypad.KScanBuf;
 if(event!=NoKey) KeyConvert((KeyScanCode *)KScanCodeTab,event);
#else
 if((Keypad.KScanBuf == NoKey) || (Keypad.KScanBuf != Keypad.KScanCode)) {
 	Keypad.KScanCode = Keypad.KScanBuf;
	Keypad.KCount = 0;
	Keypad.KHCount = 0;
	Keypad.KTimeChk = KH_val;
 	}
 else {
	Keypad.KCount++;
	if(Keypad.KCount > Keypad.KDebounceVal) {
		if(Keypad.KCount == Keypad.KDebounceVal + 3) {
			Keypad.KCount = Keypad.KDebounceVal;
			if(++Keypad.KHCount == Keypad.KTimeChk) {
				// Key hold detected
				Keypad.KScanCode &= 0xFFFE;
				KeyConvert((KeyScanCode *)KScanCodeTab, Keypad.KScanCode);
				// Change KTimeChk for key repeat
				Keypad.KTimeChk += KR_Time;
				}
			}
		}
	else if(Keypad.KCount == Keypad.KDebounceVal) {
		// Key push detected
		KeyConvert((KeyScanCode *)KScanCodeTab, Keypad.KScanCode);
		}
	}

#endif
 // Switches share one fixed injected ADC2 sequence with supply feedback.
 U16 raw_sen1,raw_sen2,raw_cpl;
 if(!PowerReadSwitches(&raw_sen1,&raw_sen2,&raw_cpl)) return;
 static dso::Selector sen1_filter,sen2_filter,cpl_filter;
 const U8 pos1=sen1_filter.update(raw_sen1);
 const U8 pos2=sen2_filter.update(raw_sen2);
 const U8 posc=cpl_filter.update(raw_cpl);
 if(pos1==255 || pos2==255 || posc==255) return;
 const U8 next_vsen=dso::sensitivity_code(pos1,pos2,VSenMin);
 const U8 next_cpl=dso::coupling_code(posc);
 if(next_vsen!=VSenPrev) { SetVSen(next_vsen); VSenPrev=next_vsen; UpdateDisp(Disp_Param); }
 if(next_cpl!=CplPrev) { SetCpl(next_cpl); CplPrev=next_cpl; UpdateDisp(Disp_Param); }
}

void	KeyConvert(KeyScanCode *KSCTab, U16 KSCode)
{
 U16 tmp1;
 
 while((tmp1 = *(U16 *)(KSCTab + 0))) {
	if(tmp1 == KSCode) {
		// -- Match found
		Keypad.KeyCode = *(U8 *)((U8 *)KSCTab + 2);
		Keypad.KeyCodeBuf = Keypad.KeyCode;
		Keypad.KeyParam = *(U8 *)((U8 *)KSCTab + 3);
		return;
		}
	else {
		// -- Proceed to next entry
		KSCTab = (KeyScanCode *)((U8 *)KSCTab + sizeof(KeyScanCode));
		}
	
 	}
}

void	DoKeyOk(void)
{
#ifdef DSO_SOURCE_ENGINE
 if(GetFocus()==FC_Generator) { ToggleGenerator(); return; }
 if(GetFocus()==FC_Cursors) { SelectCursor(); return; }
 if(GetFocus()==FC_Debug) { ToggleMeasurementPanel(); return; }
 if(GetFocus()==FC_VPos || GetFocus()==FC_TrigLvl || GetFocus()==FC_HPos) { DoKeyOkH(); return; }
#endif
 U16 tmp;
 
 tmp = GetDsoStatus();
 // Toggle HOLD state
 BitXor(tmp, DSO_Hold);

 if(BitTest(tmp, DSO_Hold)) {
 	// Set HOLD 
 	SetHold();
 	// Stop capture
 	StopCapture();
 	}
 else {
 	// Clear HOLD
 	ClrHold();
 	// Start capture at exit of HOLD
	StartCapture();
 	}
 
 UpdateDisp(Disp_Param);
}

void	DoKeyInc(void)
{
 S8	tmp0;
 S16	tmp1;
 
 switch(GetFocus()) {
#ifdef DSO_SOURCE_ENGINE
 case FC_Cursors: StepCursor(1); break;
 case FC_Generator: StepGenerator(1); break;
#endif

	case FC_Timebase:
 		tmp0 = GetTimebase();
 		tmp0++;
 		tmp0 = SetTimeBase(tmp0);
		EE_WriteVariable(VirtAddVarTab[Addr_TimeBase], tmp0);
		if(tmp0 >= TB_20ms) {
			// Restart capture
			StartCapture();
			}
		else {
			// Change sampling rate only
			UpdateTimebase();
			}
		// Make key debounce time shorter for these TB's 
		if((tmp0 < TB_20ms) && (tmp0 > TB_1s)) {
			Keypad.KDebounceVal = KD_val1;
			}
		else {
			Keypad.KDebounceVal = KD_val;
			}
	default:	
		break;
		
	case FC_TrigMode:
 		tmp0 = GetTrigMode();
 		tmp0++;
 		tmp0 = SetTrigMode(tmp0);
		EE_WriteVariable(VirtAddVarTab[Addr_TrigMode], tmp0);
		// Restart capture. 
		StartCapture();
		break;

	case FC_TrigEdge:
 		tmp0 = GetTrigEdge();
 		tmp0++;
 		tmp0 = SetTrigEdge(tmp0);
		EE_WriteVariable(VirtAddVarTab[Addr_TrigEdge], tmp0);
		break;
		
	case FC_VPos:
 		tmp1 = GetVPos();
 		tmp1++;
 		tmp1 = SetVPos(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_Vpos], tmp1);
 		UpdateDisp(Disp_Trace);
		break;

	case FC_TrigLvl:
 		tmp1 = GetTrigLvl();
#ifdef DSO_SOURCE_ENGINE
        tmp1 = StepTrigLvlPixels(1);
#else
 		tmp1++;
#endif
 		tmp1 = SetTrigLvl(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_TrigLvl], tmp1);
		OutputTLvl();
		break;
		
	case FC_HPos:
		// Move waveform right
 		tmp1 = GetHPos();
 		tmp1--;
 		tmp1 = SetHPos(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_HPos], tmp1);
 		UpdateDisp(Disp_Trace);
		break;
		
 	}
 
 UpdateDisp(Disp_Param);
}

void	DoKeyDec(void)
{
 S8	tmp0;
 S16	tmp1;
 
 switch(GetFocus()) {
#ifdef DSO_SOURCE_ENGINE
 case FC_Cursors: StepCursor(-1); break;
 case FC_Generator: StepGenerator(-1); break;
#endif

	case FC_Timebase:
 		tmp0 = GetTimebase();
 		tmp0--;
 		tmp0 = SetTimeBase(tmp0);
		EE_WriteVariable(VirtAddVarTab[Addr_TimeBase], tmp0);
		if(tmp0 >= TB_50ms) {
			// Restart capture
//			UartPutc('7', USART1);
			StartCapture();
//			UartPutc('1', USART1);
			}
		else {
			// Change sampling rate only
			UpdateTimebase();
			
			}
/*		
		// Make key debounce time shorter for these TB's 
		if((tmp0 < TB_20ms) && (tmp0 > TB_1s)) {
			Delay(50000);	// To avoid same keypress repeated
			Keypad.KDebounceVal = KD_val1;
			}
		else {
			Keypad.KDebounceVal = KD_val;
			}
*/		
	default:	
		break;
		
	case FC_TrigMode:
 		tmp0 = GetTrigMode();
 		tmp0--;
 		tmp0 = SetTrigMode(tmp0);
		EE_WriteVariable(VirtAddVarTab[Addr_TrigMode], tmp0);
		// Restart capture. 
		StartCapture();
		break;

	case FC_TrigEdge:
 		tmp0 = GetTrigEdge();
 		tmp0--;
 		tmp0 = SetTrigEdge(tmp0);
		EE_WriteVariable(VirtAddVarTab[Addr_TrigEdge], tmp0);
		break;
		
	case FC_VPos:
 		tmp1 = GetVPos();
 		tmp1--;
 		tmp1 = SetVPos(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_Vpos], tmp1);
 		UpdateDisp(Disp_Trace);
		break;

	case FC_TrigLvl:
 		tmp1 = GetTrigLvl();
#ifdef DSO_SOURCE_ENGINE
        tmp1 = StepTrigLvlPixels(-1);
#else
 		tmp1--;
#endif
 		tmp1 = SetTrigLvl(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_TrigLvl], tmp1);
		OutputTLvl();
		break;
		
	case FC_HPos:
		// Move waveform left
 		tmp1 = GetHPos();
 		tmp1++;
 		tmp1 = SetHPos(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_HPos], tmp1);
 		UpdateDisp(Disp_Trace);
		break;
		
 	}
 
 UpdateDisp(Disp_Param);
}

void	DoKeySel(void)
{
 U8 tmp;

#ifdef DSO_SOURCE_ENGINE
 const U8 order[]={FC_Timebase,FC_TrigMode,FC_TrigEdge,FC_Generator,FC_Measurements,FC_Cursors,FC_Debug,FC_TrigLvl,FC_HPos,FC_VPos};
 tmp=FC_Timebase;
 for(unsigned i=0;i<sizeof(order);++i) if(order[i]==GetFocus()) { tmp=order[(i+1)%sizeof(order)]; break; }
#else
 tmp = GetFocus(); tmp++; if(tmp>=FC_Max) tmp=0;
#endif
 SetFocus(tmp);

 UpdateDisp(Disp_Param);
}

void	DoKeyOkH(void)
{
	S16 tmp1;
 
	switch(GetFocus()) {
#ifdef DSO_SOURCE_ENGINE
 case FC_Debug: break;
 case FC_Cursors: ToggleCursorMode(); break;
 case FC_Generator: ToggleGenerator(); break;
#endif

	case FC_VPos:
		// Do VPos alignment
		tmp1 = (S16)(GetAverage() - WWindowMidValue);
		SetVPosOfs(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_VPosOfs], tmp1);
		break;

	case FC_TrigLvl:
		// Center between measured extrema; unlike the mean this is independent of duty.
#ifdef DSO_SOURCE_ENGINE
        tmp1 = (S16)(GetSignalMidpoint() - WWindowMidValue);
#else
        tmp1 = (S16)(GetAverage() - WWindowMidValue);
#endif
#ifdef DSO_SOURCE_ENGINE
        tmp1 -= GetVPosOfs();
#endif
		tmp1 = SetTrigLvl(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_TrigLvl], tmp1);
		OutputTLvl();
		break;

	case FC_HPos:
		// Auto-center horizontal position
		tmp1 = GetRecLen()/2 - WDsize/2;
		SetHPos(tmp1);
		EE_WriteVariable(VirtAddVarTab[Addr_HPos], tmp1);
		break;
	}

	UpdateDisp(Disp_Param);
}

void	DoKeyIncH(void)
{
 S16	tmp1;
 
 switch(GetFocus()) {
#ifdef DSO_SOURCE_ENGINE
 case FC_Cursors: StepCursor(held_step); break;
 case FC_Generator: StepGenerator(held_step); break;
 case FC_Timebase: DoKeyInc(); return;
#endif

	case FC_VPos:
 		tmp1 = GetVPos();
 		tmp1 +=
#ifdef DSO_SOURCE_ENGINE
        held_step;
#else
        10;
#endif
 		SetVPos(tmp1);
		break;

	case FC_TrigLvl:
 		tmp1 = GetTrigLvl();
#ifdef DSO_SOURCE_ENGINE
        tmp1 = StepTrigLvlPixels(held_step);
#else
 		tmp1 += 10;
#endif
 		SetTrigLvl(tmp1);
		OutputTLvl();
		break;
		
	case FC_HPos:
		// Move waveform right
 		tmp1 = GetHPos();
 		tmp1 -=
#ifdef DSO_SOURCE_ENGINE
        held_step;
#else
        20;
#endif
 		SetHPos(tmp1);		
		break;
 	}

 UpdateDisp(Disp_Param);
}

void	DoKeyDecH(void)
{
 S16	tmp1;
 
 switch(GetFocus()) {
#ifdef DSO_SOURCE_ENGINE
 case FC_Cursors: StepCursor(-S16(held_step)); break;
 case FC_Generator: StepGenerator(-S16(held_step)); break;
 case FC_Timebase: DoKeyDec(); return;
#endif

	case FC_VPos:
 		tmp1 = GetVPos();
 		tmp1 -=
#ifdef DSO_SOURCE_ENGINE
        held_step;
#else
        10;
#endif
 		SetVPos(tmp1);
		break;

	case FC_TrigLvl:
 		tmp1 = GetTrigLvl();
#ifdef DSO_SOURCE_ENGINE
        tmp1 = StepTrigLvlPixels(-S16(held_step));
#else
 		tmp1 -= 10;
#endif
 		SetTrigLvl(tmp1);
		OutputTLvl();
		break;
		
	case FC_HPos:
		// Move waveform left
 		tmp1 = GetHPos();
 		tmp1 +=
#ifdef DSO_SOURCE_ENGINE
        held_step;
#else
        20;
#endif
 		SetHPos(tmp1);
		break;
 	}

 UpdateDisp(Disp_Param);
}

void	DoKeySelH(void)
{
#ifdef DSO_SOURCE_ENGINE
 // Diagnostics are selected in Debug with short OK.
#endif
}

#ifndef DSO_COMMAND_HOST_TEST
void	LedBlink(void)
{
 U16	tmp;
 
 // Turn on LED
 Port_BitClr(LED_Base, (1 << LED_Bit));
 tmp = 20;
 while(tmp) {
 	Delay(65000);
 	tmp--;
 	}
 
 // Turn off LED
 Port_BitSet(LED_Base, (1 << LED_Bit));
 tmp = 20;
 while(tmp) {
 	Delay(65000);
 	tmp--;
 	}

 // Turn on LED
 Port_BitClr(LED_Base, (1 << LED_Bit));
 tmp = 20;
 while(tmp) {
 	Delay(65000);
 	tmp--;
 	}
 
 // Turn off LED
 Port_BitSet(LED_Base, (1 << LED_Bit));
}

void	TestMode(void)
{
 U16 tmp;
 
 // Change system clock to HSI
 RCC->CFGR &= ~RCC_CFGR_SW;
 RCC->CFGR |= RCC_CFGR_SW_HSI;

 // Disable JTAG and SWD
 AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG;
 AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_2;

 // Remap PD0 & PD1 to make them available as GPIO
 AFIO->MAPR |= AFIO_MAPR_PD01_REMAP;

 // Set all ports to output mode
 GPIOA->CRL = 0x33333333;
 GPIOA->CRH = 0x33333333;

 GPIOB->CRL = 0x33333333;
 GPIOB->CRH = 0x33333333;

 GPIOC->CRL = 0x33333333;
 GPIOC->CRH = 0x33333333;
 
 GPIOD->CRL = 0x33333333;
 GPIOD->CRH = 0x33333333;

 // Blink LEDs
 while(1) {
	GPIOA->ODR = 0x5555;
	GPIOB->ODR = 0x5555;
	GPIOC->ODR = 0x5555;
	GPIOD->ODR = 0x5555;

	tmp = 5;
	while(tmp) {
		Delay(60000);
		tmp--;
		}

	GPIOA->ODR = 0xAAAA;
	GPIOB->ODR = 0xAAAA;
	GPIOC->ODR = 0xAAAA;
	GPIOD->ODR = 0xAAAA;

	tmp = 5;
	while(tmp) {
		Delay(60000);
		tmp--;
		}
 	}
 
}


#endif // DSO_COMMAND_HOST_TEST
