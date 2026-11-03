// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#define DSO_HOST_TEST
#include "../src/engine.cpp"
#include "../src/test_signal.hpp"
#include <stdio.h>
#include <stdlib.h>

int checks=0;
void check(bool condition,const char* name) {
 if(!condition) { printf("FAIL %s\n",name); exit(1); }
 ++checks; printf("PASS %s\n",name);
}
void sample(uint16_t value) { adc1.SR=2; adc1.DR=value; ADC1_2_IRQHandler(); }
void block(uint16_t value,bool second=false) {
 if(!active || !(dma_channel.CCR&DMA_CCR_EN)) return;
 const unsigned count=continuous_mode?4096:512,half=count/2;
 for(unsigned i=second?half:0;i<(second?count:half);++i) dma_samples[i]=value;
 dma_channel.CNDTR=second?count:half;
 dma1.ISR=second?2:4; DMA1_Channel1_IRQHandler();
}
int main() {
 NoScopePanel=1;
 vpos=voffset=0; SetVSen(VS_2V);
 check(ycoord(2048+380)==17 && ycoord(2048+404)==17,"8 and 8.5 V at 2 V/div both meet upper viewport boundary with centered zero");
 vpos=-50;
 check(ycoord(2048)==167 && ycoord(2048+404)==61,"moving zero down preserves the corrected displacement without clipping");
 check(ycoord(2048+380)-ycoord(2048+404)==6,"nearby input levels stay distinct when viewport has room");
 vpos=0;
 check(dso::vertical_pixels(99)==25 && dso::vertical_pixels(199)==50,"5 and 10 V at 5 V/div produce one and two divisions");
 check(dso::vertical_pixels(169)==43,"8.5 V at 5 V/div produces approximately 1.7 divisions");
 check(dso::vertical_pixels(-169)==-43,"negative voltage uses symmetric vertical scale");
 check(dso::vertical_pixels(4095)==1031,"vertical scale remains valid across the ADC range");
 check(dso::vertical_pixels(404,5)==106 && dso::vertical_pixels(-404,5)==-106,"measured 2 V/div calibration maps 8.5 V to approximately 4.25 divisions symmetrically");
 bool unity=true;
 for(unsigned sen=9;sen<=12;++sen) if(dso::vertical_gain(sen)!=1078) unity=false;
 check(unity,"four ranges without accepted measurements retain their previous gain");
 vpos=-50; voffset=23; SetVSen(5);
 check(ycoord(2048+23+404)==61 && ycoord(2048+23)==167,"range gain follows zero calibration and preserves screen translation");
 triglevel=404; update_trigger_threshold();
 check(threshold==2048+23+404 && ycoord(threshold)==61,"trigger marker uses corrected range gain without changing ADC threshold");
 SetTrigLvl(200); dirty=0; SetVSen(4);
 check((dirty&Disp_Trace)!=0 && ycoord(2048+23+200)==115,"switching sensitivity invalidates trace and selects its own gain trim");
 vpos=voffset=triglevel=0; SetVSen(VS_05V); update_trigger_threshold();
 check(dso::vertical_pixels(472,6)==125 && dso::vertical_pixels(-472,6)==-125,"1 V/div observation of 4.75 divisions expands to 5 divisions symmetrically");
 check(board::test_timer_hz/(board::test_psc+1)/(board::test_arr+1)==1000 && board::test_compare*2==board::test_arr+1,
       "test output timing specifies 1 kHz and equal high/low durations");
 check(SetTimeBase(127)==24,"timebase upper clamp");
 check(SetTimeBase(-128)==1,"timebase lower clamp");
 check(SetRecLen(0)==512 && SetRecLen(65535)==1024,"record length bounds");
 SetTimeBase(TB_2ms); SetTrigMode(TM_Normal); SetTrigEdge(TE_Rising); StartCapture();
 check((tim2.PSC+1)*(tim2.ARR+1)==4480,"timer rate at 2 ms/div");
 SetTimeBase(TB_5ms); StartCapture();
 check((tim2.PSC+1)*(tim2.ARR+1)==11200,"explicit start replaces active timebase");
 SetTimeBase(TB_2ms); StartCapture();
 for(int i=0;i<600;++i) sample(2000);
 check(!(GetDsoStatus()&DSO_CaptureDone),"normal waits for trigger");
 sample(2100); for(int i=0;i<512;++i) sample(2100); DsoDisplay();
 check((GetDsoStatus()&3)==3,"rising trigger completion");
 bool ordered=true;
 for(int i=0;i<1024;++i) if(SampleBuf[i]!=(i<511?2000:2100)) ordered=false;
 check(ordered,"exact pre/post trigger ordering");
 check(GetAverage()==2050,"record mean");
 SetHold(); StartCapture(); check(tim2.CR1==0,"hold inhibits start");
 ClrHold(); SetTrigMode(TM_Single); SetTrigEdge(TE_Falling); StartCapture();
 for(int i=0;i<600;++i) sample(2100);
 sample(1900); for(int i=0;i<512;++i) sample(1900); DsoDisplay();
 check((GetDsoStatus()&7)==7,"single falling trigger holds record");
 ClrHold(); SetTrigMode(TM_Auto); StartCapture(); SystemMilliseconds=1000;
 for(int i=0;i<1536;++i) sample(2048); DsoDisplay();
 check((GetDsoStatus()&1)==1,"auto timeout with constant input");
 StopCapture(); SetTimeBase(TB_01s); StartCapture();
 for(int i=0;i<40*400;++i) sample(2222); SystemMilliseconds=2000; DsoDisplay();
 check(GetDsoStatus()&DSO_Rolling,"slow rolling mode");
 ordered=true; for(int i=984;i<1024;++i) if(SampleBuf[i]!=2222) ordered=false;
 check(ordered,"rolling retains latest samples");
 StopCapture(); SetTimeBase(TB_50ms); StartCapture();
 check(GetDsoStatus()&DSO_Rolling,"50 ms/div rolls as specified");
 check(SetVPosOfs(1000)==255,"offset bounded by library API"); SetVPosOfs(0);
 StopCapture(); SetTimeBase(TB_10us); SetTrigMode(TM_Normal); SetTrigEdge(TE_Rising); StartCapture();
 check(sample_ticks==56 && tim2.CR1==0 && (adc1.CR2&ADC_CR2_CONT),"1 MS/s fast path uses continuous ADC with TIM2 stopped");
 check((dma_channel.CCR&0xa7)==0xa7,"circular DMA configured");
 block(2000); block(2000,true); block(2100); block(2100,true); block(2100); DsoDisplay();
 check((GetDsoStatus()&3)==3,"DMA half/full trigger completion");
 ordered=true; for(int i=0;i<1024;++i) if(SampleBuf[i]!=(i<511?2000:2100)) ordered=false;
 check(ordered,"DMA sample ordering");
 for(int tb=1;tb<=24;++tb) {
  StopCapture(); SetTimeBase(tb); StartCapture();
  check(tim2.PSC<=65535 && tim2.ARR<=65535 && tim2.ARR>=1,"timebase fits hardware counter");
 }
 StopCapture(); SetRecLen(512); SetTimeBase(TB_2ms); SetTrigMode(TM_Normal); StartCapture();
 for(int i=0;i<300;++i) sample(2000);
 sample(2200); for(int i=0;i<256;++i) sample(2200); DsoDisplay();
 check(GetDsoStatus()&DSO_CaptureDone,"512 sample capture");
 StopCapture(); SetRecLen(1024); SetVPosOfs(0);
 check(SetTrigLvl(1200)==1200 && threshold==3248 && TIM4->CCR3==board::timer4_compare(3248),
       "trigger level above 300 updates software threshold and hardware PWM");
 SetTimeBase(TB_2ms); SetTrigMode(TM_Normal); SetTrigEdge(TE_Rising);
 SystemMilliseconds=4000; StartCapture(); trigger_led_active=false; GPIOA->BRR=0;
 for(int i=0;i<600;++i) sample(3100);
 check(!triggered,"high trigger waits below actual threshold");
 sample(3350);
 check(triggered && trigger_led_active && GPIOA->BRR==(1U<<15),"real high-level crossing lights active-low PA15 LED");
 SystemMilliseconds=4049; DsoTick(); check(trigger_led_active,"trigger LED remains visible for 50 ms");
 SystemMilliseconds=4050; DsoTick();
 check(!trigger_led_active && GPIOA->BSRR==(1U<<15),"SysTick turns trigger LED off after pulse");
 for(int i=0;i<512;++i) sample(3350); DsoDisplay();
 check((GetDsoStatus()&3)==3,"capture completes at threshold beyond old 300 limit");
 SetVPosOfs(255);
 check(SetTrigLvl(32767)==1792 && threshold==4095 && TIM4->CCR3==board::timer4_compare(4095),"positive calibration offset still permits ADC upper endpoint");
 check(SetTrigLvl(-32768)==-2303 && threshold==0 && TIM4->CCR3==0,"negative trigger setting cannot underflow PWM");
 SetVPosOfs(-255);
 check(SetTrigLvl(32767)==2302 && threshold==4095,"negative calibration offset permits ADC upper endpoint");
 check(SetTrigLvl(-32768)==-1793 && threshold==0,"negative calibration offset permits ADC lower endpoint");
 SetVPosOfs(0); SetTrigLvl(0); SetTrigMode(TM_Auto); StartCapture(); SystemMilliseconds=5000;
 for(int i=0;i<1536;++i) sample(2048); DsoDisplay();
 check((GetDsoStatus()&3)==1 && !trigger_led_active,"auto-timeout frame does not falsely light trigger LED");
 StopCapture(); SetTrigMode(TM_Normal); SetTrigEdge(TE_Rising); SystemMilliseconds=0xfffffff0U; StartCapture();
 for(int i=0;i<600;++i) sample(2000); sample(2100);
 SystemMilliseconds=0x21; DsoTick(); const bool before_wrap_deadline=trigger_led_active;
 SystemMilliseconds=0x22; DsoTick();
 check(before_wrap_deadline && !trigger_led_active,"LED pulse deadline handles millisecond counter wrap");
 StopCapture(); SetTimeBase(TB_20us); SetRecLen(1024); SetHPos(362); SetTrigLvl(452); SetTrigMode(TM_Normal);
 StartCapture(); const auto old_overruns=dma_overruns;
 dma1.ISR=6; DMA1_Channel1_IRQHandler();
 check(!active && !ready && last_frame_overrun && dma_overruns==old_overruns+1,"late DMA half/full flags discard incomplete frame");
 DsoDisplay(); check(active && !ready,"main loop restarts cleanly after DMA overrun");
 dma1.ISR=4; dma_channel.CNDTR=dma_continuous_count; DMA1_Channel1_IRQHandler();
 check(!active && !ready && dma_overruns==old_overruns+2,"DMA half currently being overwritten is rejected");
 StartCapture(); dma1.ISR=8; DMA1_Channel1_IRQHandler();
 check(!active && !ready && dma_overruns==old_overruns+3,"DMA transfer error cannot publish stale frame");
 bool fast_capture_ok=true,fast_alignment_ok=true;
 const int phases[]={0,17,143,374,610};
 for(int slope=0;slope<2;++slope) for(int phase:phases) {
  SetTrigEdge(slope); StartCapture();
  for(unsigned half=0;half<16 && active;++half) {
   const unsigned start=(half&1)?2048:0;
   for(unsigned i=0;i<2048;++i) dma_samples[start+i]=((half*2048+i+phase)%1000<500)?3000:2000;
   dma1.ISR=(half&1)?2:4; dma_channel.CNDTR=(half&1)?4096:2048;
   DMA1_Channel1_IRQHandler();
  }
  DsoDisplay();
  if((GetDsoStatus()&3)!=3 || last_frame_overrun) fast_capture_ok=false;
  const auto step=dso::pixel_step_q16(TB_20us,sample_ticks);
  const int64_t position=dso::viewport_start_q16(362,1024,step,false)+display_alignment_q16+150*step;
  const int index=int(position>>16),fraction=int(position&65535);
  const int interpolated=display_samples[index]+((int(display_samples[index+1])-display_samples[index])*fraction>>16);
  if(interpolated<2499 || interpolated>2501) fast_alignment_ok=false;
 }
 check(fast_capture_ok,"20 us/div captures both edges of 1 kHz square over five sampling phases");
 check(fast_alignment_ok,"fractional trigger crossing aligns to screen center at 20 us/div");
 StopCapture(); reclen=1024; hpos=362; rolling=false; timebase=TB_1ms;
 sample_ticks=dso::timing(timebase).ticks; vpos=voffset=triglevel=0;
 display_alignment_q16=0;
 update_trigger_threshold();
 for(auto& value:display_samples) value=2048;
 trace_valid=false; old_trigger_y=-1; pixel_writes=column_writes=0;
 trace();
 check(pixel_writes==63800 && column_writes==319,"initial plot and marker margins cover shifted 319x200 drawable pixels");
 pixel_writes=column_writes=0; trace();
 check(pixel_writes==0 && column_writes==0,"stationary trace sends zero pixels");
 for(auto& value:display_samples) value=2056;
 trace();
 check(pixel_writes<1500,"small signal change transfers under 2.5 percent of plot");
 bool image_correct=true;
 for(int x=0;x<300;++x) for(int y=17;y<=216;++y) {
  U16 expected=(x%25==0 || (y-17)%25==0 || x==299 || y==216)?clDimgray:clBlack;
  if(x==150) expected=clGray2;
  if(y==115) expected=clYellow;
  if(y==117) expected=x<150?clGreen:clHotpink;
  if(framebuffer[y][x+10]!=expected) image_correct=false;
 }
 check(image_correct,"differential draw restores grid and removes old trace without erase pass");
 bool moving_correct=true;
 for(int phase=0;phase<4;++phase) {
  vpos=phase*3; triglevel=phase*4;
  update_trigger_threshold();
  for(int i=0;i<1024;++i) display_samples[i]=uint16_t(2048+(((i+phase)%80)-40)*4);
  trace();
  int previous_y=0;
  for(int x=0;x<300;++x) {
   const int expected_y=117-dso::vertical_pixels((((362+x+phase)%80)-40)*4,vsen)-vpos;
   if(x==0) previous_y=expected_y;
   const int lo=expected_y<previous_y?expected_y:previous_y,hi=expected_y>previous_y?expected_y:previous_y;
   for(int y=17;y<=216;++y) {
    U16 expected=(x%25==0 || (y-17)%25==0 || x==299 || y==216)?clDimgray:clBlack;
    if(x==150) expected=clGray2;
    if(y>=lo && y<=hi) expected=clYellow;
    const int zy=117-vpos,ty=117-dso::vertical_pixels(phase*4,vsen)-vpos;
    if(y==zy) expected=clGreen;
    if(y==ty && (ty!=zy || x>=150)) expected=clHotpink;
    if(framebuffer[y][x+10]!=expected) moving_correct=false;
   }
   previous_y=expected_y;
  }
 }
 check(moving_correct,"moving sawtooth and trigger marker leave no stale pixels");
 for(auto& cache:text_cache) cache.valid=false;
 glyph_writes=0; parameters(); const auto glyphs=glyph_writes; parameters();
 check(glyphs>0 && glyph_writes==glyphs,"unchanged labels send zero extra glyphs");
 const char* expected_sensitivity[]={"5V","2V","1V","0.5V","0.2V","0.1V","50mV","20mV","10mV"};
 bool captions_ok=true;
 for(int sensitivity=4;sensitivity<=12;++sensitivity) {
  SetVSen(sensitivity); parameters();
  const char* expected=expected_sensitivity[sensitivity-4]; bool end=false;
  for(int ch=0;ch<4;++ch) {
   if(!end && !expected[ch]) end=true;
   if(text_cache[0].chars[ch]!=(end?' ':expected[ch])) captions_ok=false;
  }
 }
 check(captions_ok,"all nine actual sensitivity captions repaint when SEN1 or SEN2 changes");
 bool viewport_valid=true;
 for(unsigned tb=1;tb<=24;++tb) {
  const auto t=dso::timing(tb); const auto step=dso::pixel_step_q16(tb,t.ticks);
  const int edges[]={0,724};
  for(int hp: edges) {
   const auto start=dso::viewport_start_q16(hp,1024,step,false);
   if(start<0 || start+299*step>int64_t(1023)*65536) viewport_valid=false;
  }
 }
 check(viewport_valid,"both viewport edges stay inside samples at every timebase");
 check(!invalid_draw,"all plot and glyph writes fit 320x240 display");
 timebase=TB_20us; sample_ticks=dso::timing(timebase).ticks;
 vpos=voffset=triglevel=0; update_trigger_threshold(); display_alignment_q16=-98304;
 for(int i=0;i<1024;++i) display_samples[i]=i%100<50?2148:1948;
 trace(); int16_t mirror_top[300],mirror_bottom[300];
 for(int x=0;x<300;++x) { mirror_top[x]=old_top[x]; mirror_bottom[x]=old_bottom[x]; }
 for(auto& sample_value:display_samples) sample_value=4096-sample_value;
 trace(); bool mirror_ok=true;
 for(int x=0;x<300;++x) {
  const int top_error=mirror_top[x]+old_bottom[x]-234,bottom_error=mirror_bottom[x]+old_top[x]-234;
  if(top_error<-1 || top_error>1 || bottom_error<-1 || bottom_error>1) mirror_ok=false;
 }
 check(mirror_ok,"ideal rectangular waveform inversion preserves edge geometry within one pixel");
 vsen=VS_05V; vpos=voffset=triglevel=0; update_trigger_threshold();
 for(auto& value:display_samples) value=2048;
 SetFocus(FC_Timebase); trace();
 check(framebuffer[117][3]==clGreen && framebuffer[117][9]==clGreen && framebuffer[117][310]==clHotpink && framebuffer[117][316]==clHotpink,"left zero arrow and right trigger arrow point inward on shared level");
 bool shared_line=true;
 for(int x=10;x<310;++x) if(framebuffer[117][x]!=(x<160?clGreen:clHotpink)) shared_line=false;
 check(shared_line,"coincident one-pixel debug lines share green and pink across the full plot");
 bool scenes_ok=true;
 const int positions[]={0,-50,60,-150,150,0};
 const int levels[]={80,-100,40,0,0,-80};
 for(int scene=0;scene<6;++scene) {
  SetVPos(positions[scene]); SetTrigLvl(levels[scene]); trace();
  const int zy=117-positions[scene];
  const int ty=117-dso::vertical_pixels(levels[scene],vsen)-positions[scene];
  const int wave_y=zy<17?17:zy>216?216:zy;
  for(int x=0;x<319;++x) for(int y=17;y<=216;++y) {
   U16 expected=clBlack;
   if(x>=10 && x<310) {
    if((x-10)%25==0 || (y-17)%25==0 || x==309 || y==216) expected=clDimgray;
    if(x==160) expected=clGray2;
    if(y==wave_y) expected=clYellow;
   }
   if(y==zy) expected=clGreen;
   if(y==ty && (ty!=zy || x>=160)) expected=clHotpink;
   if(x>=3 && x<=9) {
    const int center=zy<17?17:zy>216?216:zy;
    if(y>=center-(9-x)/2 && y<=center+(9-x)/2) expected=clGreen;
   }
   if(x>=310 && x<=316) {
    const int center=ty<17?17:ty>216?216:ty;
    if(y>=center-(x-310)/2 && y<=center+(x-310)/2) expected=clHotpink;
   }
   if(zy<17 && y<=23 && x>=301-(y-17) && x<=301+(y-17)) expected=clOrange;
   if(zy>216 && y>=210 && x>=301-(216-y) && x<=301+(216-y)) expected=clOrange;
   if(framebuffer[y][x]!=expected) scenes_ok=false;
  }
 }
 check(scenes_ok,"moving and offscreen markers restore every old arrow and line pixel without stale trails");
 const int zy=117-vpos,ty=raw_ycoord(threshold);
 SetFocus(FC_VPos); trace();
 check(framebuffer[zy][9]==clAqua && framebuffer[ty][310]==clHotpink,"focus highlights only the selected zero arrow");
 SetFocus(FC_TrigLvl); trace();
 check(framebuffer[zy][9]==clGreen && framebuffer[ty][310]==clAqua,"focus highlights only the selected trigger arrow");
 pixel_writes=column_writes=0; trace();
 check(pixel_writes==0 && column_writes==0,"stationary debug lines and arrows transfer zero extra pixels");
 parameters();
 check(text_cache[6].chars[0]=='P' && text_cache[7].chars[0]=='R',"bottom panel displays measurement fields while VP and TL use side arrows");
 check(!invalid_draw,"edge-clipped arrows and lines remain inside the screen");
 SetVPos(-35); SetTrigLvl(100); SetFocus(FC_Timebase);
 for(int i=0;i<1024;++i) display_samples[i]=uint16_t(2048+((i/30)%2?140:-60));
 trace();
 FILE* preview=fopen("build/host-tests/markers.ppm","wb");
 if(preview) {
  fprintf(preview,"P6\n320 240\n255\n");
  for(int y=0;y<240;++y) for(int x=0;x<320;++x) {
   const U16 c=framebuffer[y][x];
   const unsigned char rgb[]={static_cast<unsigned char>(((c>>Rbits)&31)*255/31),static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>(((c>>Bbits)&31)*255/31)};
   fwrite(rgb,1,3,preview);
  }
  fclose(preview);
 }
 // Real publication path: ADC buffer -> snapshot -> analysis -> cached UI.
 vsen=VS_1V; voffset=0; vpos=0; reclen=1024; sample_ticks=560; continuous_mode=false;
 for(unsigned i=0;i<1024;++i) ring[i]=(i%100>=10 && i%100<60)?2248:2048;
 write_index=0; ready=true; rolling=false; status=DSO_Hold; NoScopePanel=1;
 DsoDisplay();
 check(measured.valid && measured.timing_valid && measured.frequency_uhz==1000000000 && measured.duty_permille==500,"actual snapshot publication computes Vpp RMS frequency and duty outside rendering");
 const auto frozen_frequency=measured.frequency_uhz; const auto frozen_rms=measured.rms_uv;
 SetTimeBase(TB_500s); SetRecLen(512); measurements_pending=true; DsoDisplay();
 check(measured.frequency_uhz==frozen_frequency && measured.rms_uv==frozen_rms && display_count==1024,"HOLD measurements retain recorded sample interval and record length after timebase changes");
 measurement_panel=true; measurement_readout_valid=false; SetFocus(FC_Timebase); parameters();
 check(text_cache[6].chars[0]=='P' && text_cache[7].chars[0]=='R' && text_cache[8].chars[0]=='F' && text_cache[9].chars[0]=='D',"four compact measurement fields fit the existing bottom row");
 const auto frozen_readout=measurement_readout.rms_uv;
 measured.rms_uv+=100000; SystemMilliseconds+=100; parameters();
 check(measurement_readout.rms_uv==frozen_readout,"numeric display is throttled to avoid per-frame digit flicker");
 SystemMilliseconds+=150; parameters();
 check(measurement_readout.rms_uv==measured.rms_uv,"numeric display refreshes after 250 ms");
 SetFocus(FC_Debug); ToggleMeasurementPanel(); parameters();
 check(text_cache[6].chars[0]=='A' && text_cache[7].chars[0]=='Z' && text_cache[8].chars[0]=='H' && text_cache[9].chars[0]=='v',"Debug ADC page exposes ADC mean zero offset HP and version");
 ToggleMeasurementPanel(); SetFocus(FC_HPos); parameters();
 check(hp_selected && framebuffer[219][160]==clWhite && text_cache[7].chars[0]=='H',"horizontal position displays a graphical strip and context menu");
 SetFocus(FC_Timebase); measured.clipped=true; parameters();
 check(text_cache[6].chars[3]=='C' && text_cache[7].chars[2]=='C',"ADC clipping immediately replaces voltage estimates with CLIP");
 check(!invalid_draw,"measurement and diagnostic text remain inside 320x240 display");
 // Acquisition mode transitions must not leave a continuous converter running.
 NoScopePanel=1; StopCapture(); ClrHold(); SetRecLen(1024);
 SetTimeBase(TB_20us); SetTrigMode(TM_Normal); SetTrigEdge(TE_Rising); SetTrigLvl(0); StartCapture();
 check((adc1.CR2&(ADC_CR2_ADON|ADC_CR2_CONT|ADC_CR2_DMA|ADC_CR2_EXTTRIG|ADC_CR2_SWSTART))==(ADC_CR2_ADON|ADC_CR2_CONT|ADC_CR2_DMA|ADC_CR2_EXTTRIG|ADC_CR2_SWSTART) && ((adc1.CR2>>17)&7)==7,"1 MS/s starts continuous DMA by software without recurring timer triggers");
 check(tim2.CR1==0 && board::core_hz/sample_ticks==1000000,"fast record timestamps use exact ADC conversion interval rather than disabled timer");
 SetHold();
 check(!(adc1.CR2&(ADC_CR2_ADON|ADC_CR2_CONT|ADC_CR2_DMA|ADC_CR2_SWSTART)) && dma_channel.CCR==0,"HOLD aborts continuous conversion and disables DMA completely");
 ClrHold(); SetTimeBase(TB_50us); StartCapture();
 check(!(adc1.CR2&ADC_CR2_CONT) && ((adc1.CR2>>17)&7)==3 && (tim2.CR1&1) && board::core_hz/sample_ticks==500000,"50 us/div returns to safe 500 kS/s timer-triggered DMA");
 SetTimeBase(TB_2ms); StartCapture();
 check(!dma_mode && (adc1.CR1&ADC_CR1_EOCIE) && !(adc1.CR2&ADC_CR2_CONT) && sample_ticks==4480,"slow timebase retains timer-triggered single conversions and EOC handling");
 bool all_fast_modes=true;
 const int fast_bases[]={TB_20us,TB_10us};
 const unsigned lengths[]={512,1024};
 for(int tb:fast_bases) for(unsigned edge_choice=0;edge_choice<2;++edge_choice) for(unsigned length: lengths) {
  StopCapture(); SetRecLen(length); SetTimeBase(tb); SetTrigMode(TM_Single); SetTrigEdge(edge_choice); StartCapture();
  for(unsigned half=0;half<16 && active;++half) {
   const unsigned start=(half&1)?2048:0;
   for(unsigned i=0;i<2048;++i) dma_samples[start+i]=((half*2048+i+17)%1000<500)?2200:2000;
   dma1.ISR=(half&1)?2:4; dma_channel.CNDTR=(half&1)?4096:2048;
   DMA1_Channel1_IRQHandler();
  }
  DsoDisplay();
  if(!(status&DSO_CaptureDone) || !(status&DSO_Hold) || (adc1.CR2&ADC_CR2_CONT) || display_sample_ticks!=56 || display_count!=length) all_fast_modes=false;
  ClrHold();
 }
 check(all_fast_modes,"both fast timebases preserve rising/falling SINGLE and 512/1024-record behavior at 1 MS/s");
 StopCapture(); SetRecLen(1024); SetTimeBase(TB_10us); SetTrigMode(TM_Auto); SystemMilliseconds=20000; StartCapture();
 SystemMilliseconds=20050;
 for(unsigned half=0;half<10 && active;++half) block(2048,(half&1)!=0);
 DsoDisplay();
 check((status&DSO_CaptureDone) && !(status&DSO_Trigged) && display_sample_ticks==56,"fast AUTO still publishes constant input without inventing a trigger");
 StopCapture();
 check(!(adc1.CR2&(ADC_CR2_ADON|ADC_CR2_CONT|ADC_CR2_SWSTART)) && tim2.CR1==0,"explicit stop leaves neither timer nor continuous ADC running");
 // Direct ring: valid records must survive wrap and delayed processing.
 NoScopePanel=1; SetVPosOfs(0); SetTrigLvl(0); SetTrigMode(TM_Normal); SetTrigEdge(TE_Rising); SetTimeBase(TB_10us); SetRecLen(1024);
 StartCapture();
 check(dma_channel.CNDTR==4096,"fast DMA uses 4096 physical samples without changing the 1024-sample public record");
 for(auto& value:ring) value=3333;
 block(2000); block(2100,true); DsoDisplay();
 bool ring_untouched=true,wrapped_order=true;
 for(auto value:ring) if(value!=3333) ring_untouched=false;
 for(unsigned i=0;i<1024;++i) if(display_samples[i]!=(i<511?2000:2100)) wrapped_order=false;
 check(ring_untouched && wrapped_order,"direct DMA locates the trigger and publishes exact pre/post samples without per-sample ring copies");
 const auto first_direct_pp=measured.pp_uv,first_direct_rms=measured.rms_uv;
 SetTrigLvl(300); StartCapture(); block(2200); block(2400,true); DsoDisplay();
 check(measured.pp_uv>first_direct_pp && measured.rms_uv>first_direct_rms,"successive direct records refresh PP and RMS instead of retaining old measurements");
 SetTrigLvl(0); StartCapture(); block(2000);
 for(unsigned i=2048;i<4096;++i) dma_samples[i]=2100;
 for(unsigned i=0;i<1500;++i) dma_samples[i]=2300;
 dma_channel.CNDTR=4096-1500; dma1.ISR=2; DMA1_Channel1_IRQHandler(); DsoDisplay();
 wrapped_order=true;
 for(unsigned i=0;i<1024;++i) if(display_samples[i]!=(i<511?2000:2100)) wrapped_order=false;
 check(wrapped_order && !last_frame_overrun,"delayed IRQ can publish a record across DMA wrap while its entire window remains retained");
 StartCapture(); block(2000);
 for(unsigned i=2048;i<4096;++i) dma_samples[i]=2100;
 for(unsigned i=0;i<1800;++i) dma_samples[i]=2300;
 const auto age_overruns=dma_overruns;
 dma_channel.CNDTR=4096-1800; dma1.ISR=2; DMA1_Channel1_IRQHandler();
 check(!ready && !active && dma_overruns==age_overruns+1,"overwritten pretrigger window is rejected even when the scanned DMA half is still stable");
 check((dirty&Disp_Param)!=0,"OVR rejection marks the status for immediate UI refresh");
 StartCapture(); dma_channel.CNDTR=4096; dma1.ISR=2; DMA1_Channel1_IRQHandler();
 check(!ready && !active && last_frame_overrun,"out-of-order DMA half sequence cannot publish stale raw data");
 StartCapture();
 for(auto& value:dma_samples) value=2000;
 samples_seen=0xfffff800U; filled=1024; previous=2000; dma_expected_second=true;
 for(unsigned i=2048;i<4096;++i) dma_samples[i]=2100;
 dma_channel.CNDTR=4096; dma1.ISR=2; DMA1_Channel1_IRQHandler(); DsoDisplay();
 wrapped_order=true;
 for(unsigned i=0;i<1024;++i) if(display_samples[i]!=(i<511?2000:2100)) wrapped_order=false;
 check(wrapped_order && samples_seen==0 && !last_frame_overrun,"direct record retention and posttrigger completion handle 32-bit sample-counter wrap");
 StopCapture(); SetTimeBase(TB_50us); StartCapture();
 check(!continuous_mode && dma_channel.CNDTR==512,"slower timer-DMA path retains its short 512-sample staging buffer and latency");
 StopCapture();

 StopCapture(); status=0; SetRecLen(1024); SetTimeBase(TB_50ms); SetTrigMode(TM_Auto); StartCapture();
 check(rolling && dma_mode && !continuous_mode && dma_channel.CNDTR==512 && tim2.ARR==559,
       "roll peak mode samples at 100 kS/s with the short timer DMA buffer");
 // 1 kHz, 25 percent duty: the old 500 S/s samples would all hit the same phase.
 for(unsigned j=0;j<8;++j) {
  const unsigned first=(j&1)?256:0;
  for(unsigned i=0;i<256;++i) dma_samples[first+i]=((j*256+i)%100)<25?2300:1900;
  dma_channel.CNDTR=(j&1)?512:256; dma1.ISR=(j&1)?2:4; DMA1_Channel1_IRQHandler();
 }
 check(active && !ready && filled==10 && roll_phase==48*560,
       "roll bins retain exact timebase while DMA blocks cross bin boundaries");
 bool envelope=true; for(unsigned i=0;i<10;++i) envelope &= ring[i]==1900 && roll_high[i]==2300;
 check(envelope,"1 kHz remains visible as min/max at 50 ms/div despite coherent aliasing of old samples");
 SystemMilliseconds+=100; DsoDisplay();
 check(display_peak && SampleBuf[1023]==1900 && display_high[1023]==2300,
       "roll snapshot publishes matching lower and upper envelope edges");
 trace();
 check(old_top[299]==ycoord(2300) && old_bottom[299]==ycoord(1900),"peak renderer draws both extrema at the newest visible pixel");
 auto expected_roll=dso::measure(roll_analysis,roll_analysis_count,roll_raw_ticks,vsen,2048+voffset);
 check(measured.valid && measured.mean_adc_q8==uint32_t(((525ULL*2300+1523ULL*1900)*256+1024)/2048) && measured.timing_valid && measured.frequency_uhz==1000000000ULL && measured.duty_permille==250,
       "roll RMS frequency and duty use chronological raw samples rather than min/max pairs");
 const auto saved_roll=write_index; SetHold(); block(2500); DsoDisplay();
 check(!active && write_index==saved_roll && display_peak,"HOLD freezes the peak envelope and stops roll DMA");
 ClrHold(); StopCapture(); SetTimeBase(TB_500s); StartCapture();
 for(unsigned i=0;i<1024;++i) sample(i==501?2500:2000);
 check(filled==0 && roll_min==2000 && roll_max==2500 && roll_phase==1024*560,
       "long roll intervals accumulate extrema without premature pixels or counter truncation");
 SystemMilliseconds+=100; DsoDisplay();
 check(display_high[1023]==2500 && SampleBuf[1023]==2000,"partial long roll bin shows activity before its full interval elapses");
 StopCapture(); SetTimeBase(TB_50ms); StartCapture();
 for(unsigned i=0;i<200;++i) sample(i==71?2700:2010);
 check(filled==1 && ring[0]==2010 && roll_high[0]==2700,"one raw-sample pulse survives peak reduction");
 StopCapture(); SetTimeBase(TB_1ms); StartCapture();
 for(unsigned i=0;i<600;++i) sample(1900); sample(2300);
 for(unsigned i=0;i<512;++i) sample(2300); DsoDisplay();
 check(!display_peak && (status&DSO_CaptureDone),"leaving roll restores ordinary triggered capture and rendering");

 SetFocus(FC_TrigEdge); SetTrigEdge(TE_Rising); parameters();
 check(framebuffer[11][164]==clHotpink && framebuffer[4][169]==clHotpink && framebuffer[0][162]==clAqua,
       "rising trigger icon renders low-to-high step and selected border");
 SetTrigEdge(TE_Falling); parameters();
 check(framebuffer[4][164]==clHotpink && framebuffer[11][169]==clHotpink,
       "falling trigger icon reverses the step direction");
 const auto cached_icon_columns=column_writes; parameters();
 check(column_writes==cached_icon_columns,"unchanged trigger symbol sends no repeated pixel columns");
 SetFocus(FC_Timebase); parameters();
 check(framebuffer[0][80]==clAqua && framebuffer[7][82]==clNavy && framebuffer[0][162]==clBlack,
       "active timebase gets navy background and bright border while previous selection clears");
 SetFocus(FC_HPos); hpos=0; horizontal_indicator(); const int hp_left=hp_cached;
 hpos=reclen-300; horizontal_indicator();
 check(hp_selected && hp_cached!=hp_left && framebuffer[219][160]==clWhite,"HP reference arrow stays centered while the record range moves");
 SetFocus(FC_VPos); horizontal_indicator();
 check(framebuffer[219][160]==clDimgray && !hp_selected,"HP reference persists dimmed when selection leaves horizontal position");
 SetFocus(FC_Generator); generator_hz=1000; generator_enabled=true; StepGenerator(1); parameters();
 check(generator_hz==1001 && tim3.PSC==0 && tim3.ARR==55943 && tim3.CCR2==27972 && text_cache[6].chars[0]=='S' && text_cache[7].selected,
       "generator focus exposes a highlighted frequency and configures a 1 Hz increment hardware PWM");
 const auto adc_before_gen=adc1.CR2,rail_before_gen=tim4.CCR3;
 ToggleGenerator();
 check(!generator_enabled && tim3.CCR2==0 && adc1.CR2==adc_before_gen && tim4.CCR3==rail_before_gen,
       "generator disable drives constant low without touching acquisition or inverter registers");
 generator_hz=1; StepGenerator(-1); const bool lower_bound=generator_hz==1;
 generator_hz=100000; StepGenerator(1);
 check(lower_bound && generator_hz==100000,"generator frequency presets clamp at both ends");
 ToggleGenerator();
 check(generator_enabled && tim3.ARR==559 && tim3.CCR2==280,"generator re-enable restores the selected 100 kHz frequency");
 check(!invalid_draw,"new controls and indicators remain within all display bounds");

 SetFocus(FC_Generator); generator_hz=1234; generator_enabled=true; parameters();
 check(text_cache[0].chars[0]=='1' && text_cache[1].chars[0]=='D' && text_cache[2].chars[0]!=' ' && text_cache[3].chars[0]=='A' && framebuffer[4][180]==clGreen,
       "header places voltage coupling timebase trigger and generator before the independent supply field");
 check(text_cache[7].chars[0]=='F' && text_cache[7].chars[1]=='1' && text_cache[7].chars[2]=='2' && text_cache[7].chars[3]=='3' && text_cache[7].chars[4]=='4' && text_cache[8].chars[0]=='=',
       "generator context shows integer-Hz target and separate divider-derived actual frequency");
 SetFocus(FC_HPos); parameters();
 check(text_cache[7].chars[0]=='H' && text_cache[6].chars[0]=='+' && hp_selected,"HP context and viewport indicator appear beneath the raster");
 SetFocus(FC_Timebase); parameters();
 check(text_cache[6].chars[0]=='P' && !hp_selected,"leaving context restores measurements and clears the HP indicator");

 SetFocus(FC_Generator); parameters();
 check(text_cache[9].chars[0]=='P' && text_cache[9].chars[1]=='5' && text_cache[9].chars[2]=='0',"generator footer exposes independent inverter PWM duty telemetry");
 SetVSen(VS_10mV); parameters();
 check(glyph_at[0][10]=='1' && glyph_at[0][34]=='V',"longest voltage label starts ten pixels from the panel edge and fits inside its padded box");
 SetVSen(VS_1V); parameters();
 check(glyph_at[0][18]=='1' && glyph_at[0][26]=='V',"short voltage text is pixel-centered in the same padded field");

 status=0; last_frame_overrun=false; trigger_badge_active=false; status_badge_cache=-1; status_badge();
 check(framebuffer[0][224]==clGreen && glyph_at[0][224]=='A',"free-running AUTO state is clearly labelled on green background");
 SystemMilliseconds=1000; mark_real_trigger(2000,2100,2048); status_badge();
 const auto badge_pixels=pixel_writes; status&=~DSO_Trigged; SystemMilliseconds=1200; DsoTick(); status_badge();
 check(framebuffer[0][224]==clRed && glyph_at[0][228]=='T' && pixel_writes==badge_pixels,"trigger badge stays red for 300 ms across acquisition restarts without repainting");
 mark_real_trigger(2000,2100,2048); SystemMilliseconds=1499; DsoTick(); status_badge();
 check(trigger_badge_active && framebuffer[0][224]==clRed,"new real trigger extends status retention independently of frame flags");
 SystemMilliseconds=1500; DsoTick(); status_badge();
 check(!trigger_badge_active && framebuffer[0][224]==clGreen,"run indication returns after the trigger retention expires");
 SystemMilliseconds=0xfffffff0; mark_real_trigger(2000,2100,2048); SystemMilliseconds=100; DsoTick();
 const bool retained_wrap=trigger_badge_active; SystemMilliseconds=284; DsoTick();
 check(retained_wrap && !trigger_badge_active,"trigger retention handles millisecond counter wrap");
 status=DSO_Hold; status_badge(); const bool hbadge=glyph_at[0][224]=='H';
 status=0; last_frame_overrun=true; status_badge();
 check(hbadge && glyph_at[0][228]=='O',"HOLD and OVR override ordinary retained R/T symbols"); last_frame_overrun=false;
 SetFocus(FC_HPos); hpos=0; horizontal_indicator(); const bool left_arrow=framebuffer[219][160]==clWhite && framebuffer[219][310]==clAqua && framebuffer[219][10]==clBlack;
 hpos=reclen-300; horizontal_indicator();
 check(left_arrow && framebuffer[219][160]==clWhite && framebuffer[219][10]==clAqua && framebuffer[219][310]==clBlack,
       "HP arrow stays fixed while both full-range endpoints move beneath it");
 NoScopePanel=1; StopCapture(); status=0; SetRecLen(1024); SetTimeBase(TB_50ms); SetTrigMode(TM_Normal); SetTrigEdge(TE_Rising); SetTrigLvl(0); SetVPosOfs(0); StartCapture();
 for(unsigned i=0;i<102400;++i) sample(2000);
 SampleBuf[1023]=1234; SystemMilliseconds+=100; DsoDisplay();
 check(active && !ready && !triggered && SampleBuf[1023]==1234,"50 ms normal roll waits for a real trigger instead of publishing free-running snapshots");
 sample(2100); for(unsigned i=0;i<102399;++i) sample(2100);
 check(ready && !active && triggered,"50 ms normal peak-roll completes the posttrigger half-record");
 DsoDisplay(); check(display_peak && (status&DSO_CaptureDone) && !(status&DSO_Hold),"normal roll publishes a triggered min/max frame without forcing HOLD");
 StopCapture(); status=0; SetTimeBase(TB_01s); SetTrigMode(TM_Single); SetTrigEdge(TE_Falling); StartCapture();
 for(unsigned i=0;i<204800;++i) sample(2100);
 sample(1900); for(unsigned i=0;i<204799;++i) sample(1900); DsoDisplay();
 check((status&(DSO_Hold|DSO_Trigged|DSO_CaptureDone))==(DSO_Hold|DSO_Trigged|DSO_CaptureDone) && !active,"100 ms single peak-roll captures a falling edge then holds");
 status=0; SetTrigMode(TM_Auto); SetTimeBase(TB_50ms); StartCapture();
 for(unsigned i=0;i<2000;++i) sample(i%100<50?2000:2100);
 check(active && !ready && trigger_badge_active,"AUTO peak-roll continues streaming while reporting real trigger activity");

 SetFocus(FC_TrigMode); SetTrigMode(TM_Normal); parameters();
 check(text_cache[3].chars[0]=='N' && text_cache[7].chars[0]=='N' && text_cache[7].chars[1]=='O',"full NORM trigger label retains the full NORMAL label in its context");
 SetTrigMode(TM_Single); parameters(); check(text_cache[3].chars[0]=='S',"single mode uses SING label");
 SetTrigMode(TM_Auto); parameters(); check(text_cache[3].chars[0]=='A',"auto mode uses AUTO label");
 SetTimeBase(TB_02ms); parameters();
 check(text_cache[2].chars[0]=='2' && text_cache[2].chars[3]=='u' && framebuffer[7][72]==clGreen,"timebase uses horizontal time symbol and compact integer units");

 SetFocus(FC_Measurements); measured=dso::Measurements{}; measured.valid=true; measured.maximum_uv=3300000; measured.minimum_uv=-125000; measured.mean_uv=825000; measured.timing_valid=true; measured.period_ns=1000000; measured.high_ns=250000;
 measurement_readout_valid=false; parameters();
 check(text_cache[10].chars[0]=='M' && text_cache[10].chars[1]=='x' && text_cache[11].chars[1]=='n' && text_cache[12].chars[0]=='A' && text_cache[13].chars[0]=='P' && text_cache[14].chars[0]=='T',"measurement menu displays all five extended values together");
 SetFocus(FC_Timebase); parameters(); check(text_cache[6].valid && !text_cache[10].valid,"leaving the measurement menu restores ordinary footer layout");
 trigmode=TM_Normal; status=0; active=false; trigger_badge_active=false; status_badge();
 check(glyph_at[0][224]=='W',"untriggered normal/single state explicitly says WAIT");

 SetRecLen(1024); SetHPos(0); const bool norm_left=normalized_hpos()==-1000;
 SetHPos(362); const bool norm_center=normalized_hpos()==0; SetHPos(724);
 check(norm_left && norm_center && normalized_hpos()==1000,"HP normalizes left center right to minus one zero plus one");
 SetRecLen(513); SetHPos(106); check(normalized_hpos()==0,"odd record lengths preserve an exact normalized center");
 char hp_label[11]; format_hpos(hp_label); check(hp_label[2]=='0' && hp_label[3]=='.',"normalized HP label uses decimal units rather than sample indices");
 SetFocus(FC_HPos); horizontal_indicator(); trace();
 check(framebuffer[217][160]==clWhite && framebuffer[217][158]==clBlack && framebuffer[219][158]==clWhite,"fixed HP arrow points upward with narrow tip above wide base");
 check(framebuffer[18][160]==clAqua && framebuffer[210][160]==clAqua,"HP reference line spans the raster at the fixed pointer position");

 StopCapture(); status=DSO_Hold; rolling=false; display_peak=false; SetRecLen(1024); SetTimeBase(TB_1ms); sample_ticks=dso::timing(timebase).ticks;
 hpos=362; vpos=voffset=triglevel=0; SetVSen(6); update_trigger_threshold();
 for(unsigned i=0;i<1024;++i) display_samples[i]=i%100<50?2300:1900;
 SetFocus(FC_Timebase); trace_valid=false; trace();
 auto plot_hash=[]() { uint32_t h=2166136261U; for(int y=17;y<=216;++y) for(int x=10;x<310;++x) h=(h^framebuffer[y][x])*16777619U; return h; };
 const auto before_cursors=plot_hash();
 cursor_voltage=false; cursor_selected=0; cursor_x[0]=85; cursor_x[1]=235; SetFocus(FC_Cursors); trace(); parameters();
 check(framebuffer[200][85]==clOrange && framebuffer[200][235]==clDeepskyblue,"time cursors draw independent active and inactive reference lines");
 check(text_cache[8].chars[0]=='d' && text_cache[8].chars[1]=='T' && text_cache[8].chars[2]=='6',"time cursor footer reports the calibrated six-division delta time");
 StepCursor(1); trace(); check(framebuffer[200][86]==clOrange && framebuffer[200][85]!=clOrange,"moving a time cursor restores its old column");
 SelectCursor(); trace(); check(framebuffer[200][235]==clOrange && framebuffer[200][86]==clDeepskyblue,"short cursor selection swaps active-line colours");
 ToggleCursorMode(); trace(); check(framebuffer[167][120]==clOrange && framebuffer[67][120]==clDeepskyblue,"voltage cursor mode replaces time lines with two horizontal references");
 StepCursor(1); trace(); check(framebuffer[166][120]==clOrange && framebuffer[167][120]!=clOrange,"moving voltage cursor restores the previous row");
 SetFocus(FC_Timebase); trace(); check(plot_hash()==before_cursors,"leaving cursor mode restores every original plot pixel without stale lines");
 have_measurement_frame=true; measured.valid=true; measured.clipped=false; measured.minimum=1900; measured.maximum=2300; average=2000; voffset=23;
 check(GetSignalMidpoint()==2100,"trigger centering uses extrema midpoint instead of duty-weighted mean");
 dso::ExportFrame exported; check(DsoReadExportFrame(&exported) && exported.samples==display_samples && exported.count>=512,"export reads acquisition samples rather than rendered cursor pixels");
 printf("%d checks passed (source engine, simulated registers and framebuffer).\n",checks);
}





