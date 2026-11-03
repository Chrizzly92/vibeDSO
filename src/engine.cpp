// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
// Source implementation of the documented DSO138 control API.
// Hardware wiring comes from the supplied JYE source package. This is a new
// implementation, NOT a claim to have recovered the complete 061 algorithms.
#include "engine_math.hpp"
#include "measurements.hpp"
#include "stream_measurements.hpp"
#include "link.hpp"
#include "acquisition_config.hpp"
#include "test_signal.hpp"
#include "power.hpp"
#ifdef DSO_HOST_TEST
#include "host_mmio.hpp"
#endif

extern "C" void DMA1_Channel1_IRQHandler(void);
extern "C" void ADC1_2_IRQHandler(void);
extern "C" void TIM1_CC_IRQHandler(void);
extern volatile uint32_t SystemMilliseconds;

namespace {
using dso::clamp;
constexpr unsigned dma_timer_count=dso::timer_dma_samples,dma_continuous_count=dso::continuous_dma_samples;
alignas(4) volatile uint16_t dma_samples[dma_continuous_count];
bool continuous_mode=false,dma_expected_second=false;
uint32_t dma_capture_end_total=0;
uint16_t ring[1024], display_samples[1024];
// Roll mode uses only 512 DMA staging words. Reuse its otherwise idle tail.
volatile uint16_t* const roll_high=dma_samples+512;
volatile uint16_t* const display_high=dma_samples+1536;
volatile uint16_t* const roll_raw=dma_samples+2560;
uint16_t* const roll_analysis=const_cast<uint16_t*>(dma_samples+3072);
uint64_t roll_phase=0;
uint16_t roll_min=4095,roll_max=0,roll_raw_write=0,roll_raw_count=0;
unsigned roll_analysis_count=0;
bool display_peak=false;
constexpr uint64_t roll_raw_ticks=board::core_hz/100000;
dso::StreamMeter roll_meter;
dso::StreamView roll_meter_view;
uint32_t frame_sequence=0,frame_published_ms=0;
bool frame_partial_peak=false;
uint16_t frame_status=0,frame_zero=2048,frame_gain=1024;
uint8_t frame_sensitivity=4,frame_coupling=0;
int8_t capture_sensitivity=VS_05V,capture_coupling=CP_DC;

volatile uint16_t write_index, filled, post_count, status=DSO_NormalDisplay;
volatile bool active, ready, triggered, rolling, dma_mode;
volatile uint32_t samples_seen;
volatile int threshold=2048;
volatile bool rising;
uint16_t previous=2048;
int8_t vsen=VS_05V, coupling=CP_DC, timebase=TB_1ms, trigmode=TM_Auto, edge=TE_Falling;
int16_t vpos=0,hpos=362,triglevel=0,voffset=0;
uint16_t reclen=1024,average=2048;
uint8_t focus=FC_Timebase;
uint32_t generator_hz=1000;
bool generator_enabled=true;
bool cursor_voltage=false; unsigned cursor_selected=0;
int cursor_x[2]={85,235},cursor_y[2]={67,167};
bool old_cursors=false,old_cursor_voltage=false; unsigned old_cursor_selected=0;
int old_cursor_x[2]={85,235},old_cursor_y[2]={67,167};
uint16_t dirty=Disp_Panel|Disp_Param|Disp_Trace;
uint64_t sample_ticks=board::core_hz/25000;
uint32_t started_ms,last_roll_ms;
bool painted;
int32_t capture_alignment_q16=0,display_alignment_q16=0;
volatile uint32_t dma_overruns=0;
volatile bool last_frame_overrun=false;
volatile bool trigger_led_enabled=false,trigger_led_active=false;
volatile uint32_t trigger_led_until=0;
volatile bool trigger_badge_active=false;
volatile uint32_t trigger_badge_until=0;
uint32_t roll_last_trigger_ms=0;
int16_t old_top[300],old_bottom[300];
bool trace_valid;
uint8_t old_clip_flags=0;
int old_trigger_y=-1;
int old_zero_y=-1;
uint8_t old_marker_focus=255;
uint32_t last_display_ms;
bool ui_dirty=false; uint32_t ui_sequence=0;
unsigned rendered_timebase=TB_1ms;
dso::Measurements measured,measurement_readout;
bool measurement_readout_valid=false;
uint32_t last_measurement_text_ms=0;
bool measurements_pending=false,have_measurement_frame=false;
uint64_t display_sample_ticks=board::core_hz/25000;
unsigned display_count=1024;
bool measurement_panel=true;

struct Critical {
 uint32_t saved;
 Critical() {
#ifdef DSO_HOST_TEST
  saved=0;
#else
  __asm volatile("mrs %0, primask\ncpsid i" : "=r"(saved) :: "memory");
#endif
 }
 ~Critical() {
#ifndef DSO_HOST_TEST
  __asm volatile("msr primask, %0" :: "r"(saved) : "memory");
#endif
 }
};
void update_trigger_threshold() {
 triglevel=clamp<int>(triglevel,-2048-voffset,2047-voffset);
 threshold=2048+voffset+triglevel;
 TIM4->CCR3=board::timer4_compare(uint32_t(threshold));
 dirty |= Disp_Trace|Disp_Param;
}
void stop_hardware() {
 TIM2->CR1=0;
 ADC1->CR1=0;
 ADC1->CR2 &= ~(ADC_CR2_ADON|ADC_CR2_CONT|ADC_CR2_SWSTART|ADC_CR2_EXTTRIG|ADC_CR2_DMA);
 DMA1_Channel1->CCR=0;
 active=false;
}
void mark_real_trigger(uint16_t before,uint16_t sample,int level) {
 // Crossed sample is record index N/2-1, its predecessor N/2-2.
 // Fractional crossing aligns fast views instead of rounding to a sample.
 const int difference=int(sample)-before;
 const int fraction=difference?clamp<int>(((level-int(before))*65536)/difference,0,65536):65536;
 capture_alignment_q16=fraction-2*65536;
 status |= DSO_Trigged;
 trigger_badge_active=true; trigger_badge_until=SystemMilliseconds+300;
 dirty|=Disp_Param;
 if(trigger_led_enabled) {
  GPIOA->BRR=1U<<15; trigger_led_until=SystemMilliseconds+50; trigger_led_active=true;
 }
}
// Called only from IRQ context. Ring contents are frozen before publication.
void accept(uint16_t sample) {
 if(!active) return;
 if(rolling) {
  roll_meter.push(sample);
  const bool crossing=dso::crossing(previous,sample,threshold,rising);
  if(trigmode==TM_Auto) {
   if(crossing && (!trigger_badge_active || uint32_t(SystemMilliseconds-roll_last_trigger_ms)>=50)) {
    mark_real_trigger(previous,sample,threshold); roll_last_trigger_ms=SystemMilliseconds;
   }
  } else if(!triggered && filled>=reclen/2 && crossing) {
   triggered=true; post_count=reclen/2; mark_real_trigger(previous,sample,threshold);
  }
  roll_raw[roll_raw_write]=sample; roll_raw_write=(roll_raw_write+1)&511;
  if(roll_raw_count<512) ++roll_raw_count;
  if(sample<roll_min) roll_min=sample;
  if(sample>roll_max) roll_max=sample;
  roll_phase+=roll_raw_ticks;
  if(roll_phase>=sample_ticks) {
   roll_phase-=sample_ticks;
   ring[write_index]=roll_min; roll_high[write_index]=roll_max;
   write_index=(write_index+1)&1023;
   if(filled<reclen) ++filled;
   ++samples_seen; roll_min=4095; roll_max=0;
   if(trigmode!=TM_Auto && triggered && post_count && !--post_count) {
    stop_hardware(); __DMB(); ready=true;
   }
  }
  previous=sample; return;
 }
 ring[write_index]=sample;
 write_index=(write_index+1)&1023;
 if(filled<reclen) ++filled;
 ++samples_seen;

 if(triggered) {
  if(post_count) --post_count;
  if(!post_count) {
   stop_hardware();
   __DMB(); ready=true;
  }
 } else if(filled>=reclen/2) {
  const bool hit=dso::crossing(previous,sample,threshold,rising);
  // Auto mode publishes a free-running record after at least 40 ms.
  const bool automatic=trigmode==TM_Auto && filled>=reclen &&
                       uint32_t(SystemMilliseconds-started_ms)>=40;
  if(hit || automatic) {
   triggered=true;
   if(hit) mark_real_trigger(previous,sample,threshold);
   post_count=reclen-reclen/2;
  }
 }
 previous=sample;
}
template<bool Up> void process_dma_half(unsigned first) {
 // Roll peak detection uses timer DMA at 100 kS/s. Keep the branch outside
 // paying a rolling-mode branch for every sample in the 1 MS/s hot loop.
 if(rolling) { for(unsigned i=first;i<first+256;++i) accept(dma_samples[i]); return; }
 // Main-loop setters cannot run during this IRQ. Keep hot state in registers
 // for the whole half-buffer rather than reloading volatile fields per sample.
 unsigned wi=write_index,fi=filled,pc=post_count;
 bool hit_already=triggered,complete=false;
 uint16_t prev=previous;
 const unsigned length=reclen,pre=length/2;
 const int level=threshold;
 constexpr bool up=Up;
 const bool auto_due=trigmode==TM_Auto && uint32_t(SystemMilliseconds-started_ms)>=40;
 unsigned consumed=0;
 for(unsigned i=first;i<first+256;++i) {
  const uint16_t sample=dma_samples[i];
  ring[wi]=sample; wi=(wi+1)&1023; if(fi<length) ++fi; ++consumed;
  {
   if(hit_already) { if(pc) --pc; if(!pc) complete=true; }
   else if(fi>=pre) {
    const bool hit=dso::crossing(prev,sample,level,up);
    if(hit || (auto_due && fi>=length)) {
     hit_already=true; pc=length-pre;
     if(hit) mark_real_trigger(prev,sample,level);
    }
   }
  }
  prev=sample;
  if(complete) break;
 }
 write_index=wi; filled=fi; post_count=pc; triggered=hit_already;
 previous=prev; samples_seen+=consumed;
 if(complete) { stop_hardware(); __DMB(); ready=true; }
}
void reject_dma_frame() {
 stop_hardware(); ready=false; status&=~(DSO_CaptureDone|DSO_Trigged);
 ++dma_overruns; last_frame_overrun=true; dirty|=Disp_Param;
}
bool dma_half_available(bool second,uint32_t remaining) {
 const unsigned count=continuous_mode?dma_continuous_count:dma_timer_count;
 if(remaining>count) return false;
 return second?(remaining>count/2 || remaining==0):(remaining>0 && remaining<=count/2);
}
// Fast DMA writes directly into a four-record circular buffer. Scan for the
// trigger only; never copy each sample into a second ring from this interrupt.
template<bool Up> void process_direct_dma_half(unsigned first) {
 constexpr unsigned half=dma_continuous_count/2;
 const uint32_t base=samples_seen;
 if(!triggered) {
  const unsigned pre=reclen/2;
  const unsigned begin=filled>=pre?0:pre-filled-1;
  const bool auto_due=trigmode==TM_Auto && uint32_t(SystemMilliseconds-started_ms)>=40;
  const unsigned auto_at=!auto_due?half:filled>=reclen?0:reclen-filled-1;
  uint16_t prev=begin?dma_samples[first+begin-1]:previous;
  const int level=threshold;
  for(unsigned j=begin;j<half;++j) {
   const uint16_t sample=dma_samples[first+j];
   const bool hit=dso::crossing(prev,sample,level,Up);
   if(hit || j==auto_at) {
    triggered=true;
    if(hit) mark_real_trigger(prev,sample,level);
    dma_capture_end_total=base+j+1+reclen/2;
    break;
   }
   prev=sample;
  }
 }
 samples_seen=base+half;
 filled=filled+half<reclen?filled+half:reclen;
 previous=dma_samples[first+half-1];
 const int32_t pending=int32_t(dma_capture_end_total-samples_seen);
 post_count=triggered && pending>0?uint16_t(pending):0;
 if(triggered && pending<=0) {
  stop_hardware();
  const unsigned written=(dma_continuous_count-DMA1_Channel1->CNDTR)&(dma_continuous_count-1);
  const unsigned age=(written-(dma_capture_end_total&(dma_continuous_count-1)))&(dma_continuous_count-1);
  // Keep four extra samples of margin for a final in-flight DMA transfer.
  if(age>dma_continuous_count-reclen-4) { reject_dma_frame(); return; }
  __DMB(); ready=true;
 }
}
void snapshot() {
 const bool partial=rolling && roll_min<=roll_max;
 const uint16_t start=(write_index+1024-reclen+(partial?1:0))&1023;
 uint32_t sum=0;
 for(uint16_t i=0;i<reclen;++i) {
  display_samples[i]=continuous_mode
    ?dma_samples[(dma_capture_end_total+dma_continuous_count-reclen+i)&(dma_continuous_count-1)]
    :ring[dso::ring_index(start,i)];
  if(rolling) {
   display_high[i]=roll_high[dso::ring_index(start,i)];
   if(partial && i==reclen-1) { display_samples[i]=roll_min; display_high[i]=roll_max; }
  }
  sum+=display_samples[i];
 }
 display_peak=rolling;
 if(rolling) {
  roll_analysis_count=roll_raw_count;
  sum=0;
  for(unsigned i=0;i<roll_analysis_count;++i) {
   roll_analysis[i]=roll_raw[(roll_raw_write+512-roll_analysis_count+i)&511];
   sum+=roll_analysis[i];
  }
 }
 average=sum/(rolling?(roll_analysis_count?roll_analysis_count:1):reclen);
 if(rolling) roll_meter_view=roll_meter.view();
 frame_partial_peak=partial;
 ++frame_sequence; frame_published_ms=SystemMilliseconds; frame_status=status;
 frame_zero=2048+voffset; frame_sensitivity=capture_sensitivity; frame_coupling=capture_coupling;
 frame_gain=dso::vertical_gain(frame_sensitivity);
 display_sample_ticks=sample_ticks; display_count=reclen; have_measurement_frame=true; measurements_pending=true;
 display_alignment_q16=rolling?0:capture_alignment_q16;
 last_frame_overrun=false;
 CurrentSample=display_samples+clamp<int>(hpos,0,reclen-1);
 dirty |= Disp_Trace|Disp_Param;
}
struct TextCache { char chars[10]; uint16_t color; bool valid; bool selected; uint16_t x; uint8_t y,box_width,length; };
TextCache text_cache[15];
void text(unsigned slot,int x,int y,unsigned width,const char* s,uint16_t color=clWhite,bool selected=false) {
 TextCache& cache=text_cache[slot];
 const int padding=slot<4?2:0,box_width=width*8+padding*2;
 cache.x=x; cache.y=y; cache.box_width=box_width;
 unsigned length=0; while(length<width && s[length]) ++length;
 cache.length=length;
 bool changed=!cache.valid || cache.color!=color || cache.selected!=selected;
 for(unsigned i=0;i<width;++i) {
  const char ch=i<length?s[i]:' ';
  changed |= cache.chars[i]!=ch; cache.chars[i]=ch;
 }
 if(changed) {
  ui_dirty=true;
  FillRect(x,y,box_width,16,selected?clNavy:clBlack);
  const int start=x+(box_width-int(length)*8)/2;
  for(unsigned i=0;i<length;++i)
   PutcGenic(start+int(i)*8,y,U8(s[i]),color,selected?clNavy:clBlack,&ASC8X16);
  if(selected) {
   FillRect(x,y,box_width,1,clAqua); FillRect(x,y+15,box_width,1,clAqua);
   FillRect(x,y,1,16,clAqua); FillRect(x+box_width-1,y,1,16,clAqua);
  }
 }
 cache.color=color; cache.valid=true; cache.selected=selected;
}

const char* tb_labels[]={"?","500s","200s","100s","50s","20s","10s","5s","2s","1s",
 "500ms","200ms","100ms","50ms","20ms","10ms","5ms","2ms","1ms","500us","200us","100us","50us","20us","10us"};
const char* vs_labels[]={"?","50V","20V","10V","5V","2V","1V","0.5V","0.2V","0.1V","50mV","20mV","10mV"};
// Cached 24x16 step-edge glyph: no font lookup or bitmap-sized RAM buffer.
int icon_edge=-1,icon_focus=-1;
void edge_icon() {
 const bool selected=focus==FC_TrigEdge;
 if(icon_edge==edge && icon_focus==int(selected)) return;
 ui_dirty=true;
 U16 column[16];
 for(int x=0;x<10;++x) {
  for(int y=0;y<16;++y) {
   const int before=edge==TE_Rising?11:4,after=edge==TE_Rising?4:11;
   const bool stroke=(x>=2 && x<5 && y==before) || (x==5 && y>=4 && y<=11) || (x>=5 && x<=7 && y==after);
   const bool border=selected && (x==0 || x==9 || y==0 || y==15);
   column[y]=border?clAqua:stroke?clHotpink:selected?clNavy:clBlack;
  }
  WriteColumn(162+x,0,column,16);
 }
 icon_edge=edge; icon_focus=selected;
}
int generator_icon_state=-1;
void generator_icon() {
 const int state=int(generator_enabled)+2*int(focus==FC_Generator);
 if(generator_icon_state==state) return;
 ui_dirty=true;
 U16 column[16];
 for(int x=0;x<16;++x) {
  for(int y=0;y<16;++y) {
   const bool stroke=(y==11 && ((x>=2 && x<=5) || (x>=10 && x<=13))) || (y==4 && x>=5 && x<=10) || ((x==5 || x==10) && y>=4 && y<=11);
   const bool selected=focus==FC_Generator;
   column[y]=selected && (x==0 || x==15 || y==0 || y==15)?clAqua:stroke?(generator_enabled?clGreen:clDimgray):selected?clNavy:clBlack;
  }
  WriteColumn(174+x,0,column,16);
 }
 generator_icon_state=state;
}
int normalized_hpos() {
 const int maximum=reclen-300,center=maximum/2;
 const int delta=int(hpos)-center,denominator=delta<=0?center:maximum-center;
 return (delta*1000+(delta<0?-denominator/2:denominator/2))/denominator;
}
void format_hpos(char out[11]) {
 const int raw=normalized_hpos();
 const unsigned magnitude=(unsigned(raw<0?-raw:raw)+5)/10;
 dso::format_fixed(out,magnitude?(raw<0?"HP-":"HP+"):"HP",magnitude,2,"");
}
int hp_cached=-1,hp_length=-1; uint64_t hp_ticks=0; int hp_timebase=-1; bool hp_selected=false;
void horizontal_indicator() {
 const bool selected=focus==FC_HPos;
 if(selected==hp_selected && hp_cached==hpos && hp_length==reclen && hp_ticks==sample_ticks && hp_timebase==timebase) return;
 ui_dirty=true;
 FillRect(0,217,320,5,clBlack);
 {
  const U16 bar_color=selected?clAqua:clDimgray,arrow_color=selected?clWhite:clDimgray;
  // Screen center is the fixed reference; the record range moves underneath.
  constexpr int center=160;
  const int left=center-int(int32_t(hpos)*150/(reclen-300));
  const int right=left+150;
  FillRect(left,219,right-left+1,1,bar_color);
  FillRect(left,218,1,3,bar_color); FillRect(right,218,1,3,bar_color);
  FillRect(center,217,1,1,arrow_color); FillRect(center-1,218,3,1,arrow_color);
  FillRect(center-2,219,5,1,arrow_color); FillRect(center,220,1,2,arrow_color);
 }
 hp_cached=hpos; hp_length=reclen; hp_ticks=sample_ticks; hp_timebase=timebase; hp_selected=selected;
}
int cursor_icon_cache=-1;
void cursor_icon() {
 const int selected=focus==FC_Cursors;
 if(cursor_icon_cache==selected) return;
 ui_dirty=true;
 FillRect(208,0,16,16,selected?clNavy:clBlack);
 PutcGenic(212,0,'C',clWhite,selected?clNavy:clBlack,&ASC8X16);
 if(selected) { FillRect(208,0,16,1,clAqua); FillRect(208,15,16,1,clAqua); FillRect(208,0,1,16,clAqua); FillRect(223,0,1,16,clAqua); }
 cursor_icon_cache=selected;
}
int32_t cursor_voltage_uv(int y) { return dso::signed_codes_to_uv(dso::adc_delta_for_pixels(117-vpos-y,vsen)*256,vsen); }
int debug_icon_cache=-1;
void debug_icon() {
 char label[7]; U16 color; PowerReadDisplay(label,&color);
 const bool selected=focus==FC_Debug, fault=color==clRed;
 const int state=int(selected)+2*int(fault);
 if(debug_icon_cache==state) return;
 ui_dirty=true;
 const U16 bg=selected?clNavy:clBlack;
 FillRect(256,0,56,16,bg);
 const char* title=fault?"PWR!":"D";
 const int length=fault?4:1;
 for(int i=0;i<length;++i) PutcGenic(256+(56-length*8)/2+i*8,0,title[i],fault?clRed:clWhite,bg,&ASC8X16);
 if(selected) { FillRect(256,0,56,1,clAqua); FillRect(256,15,56,1,clAqua); FillRect(256,0,1,16,clAqua); FillRect(311,0,1,16,clAqua); }
 debug_icon_cache=state;
}
int status_badge_cache=-1,measurement_icon_cache=-1;
void measurement_icon() {
 const int selected=focus==FC_Measurements;
 if(measurement_icon_cache==selected) return;
 ui_dirty=true;
 FillRect(192,0,16,16,selected?clNavy:clBlack);
 PutcGenic(196,0,'M',clWhite,selected?clNavy:clBlack,&ASC8X16);
 if(selected) { FillRect(192,0,16,1,clAqua); FillRect(192,15,16,1,clAqua); FillRect(192,0,1,16,clAqua); FillRect(207,0,1,16,clAqua); }
 measurement_icon_cache=selected;
}
void status_badge() {
 const int state=status&DSO_Hold?2:last_frame_overrun?3:(trigger_badge_active || (active && triggered))?1:trigmode==TM_Auto?0:4;
 if(state==status_badge_cache) return;
 ui_dirty=true;
 const U16 background=state==1 || state==3?clRed:state==0?clGreen:state==2?clOrange:clNavy;
 FillRect(224,0,32,16,background);
 const char* label=state==0?"AUTO":state==1?"T'D":state==2?"HOLD":state==3?"OVR":"WAIT";
 const unsigned length=state==1?3:state==3?3:4;
 for(unsigned i=0;i<length;++i) PutcGenic(224+(32-int(length)*8)/2+8*i,0,label[i],state==0?clBlack:clWhite,background,&ASC8X16);
 status_badge_cache=state;
}
bool time_symbol_valid=false;
void time_symbol() {
 if(time_symbol_valid) return;
 ui_dirty=true;
 FillRect(72,7,8,1,clGreen);
 FillRect(72,6,1,3,clGreen); FillRect(79,6,1,3,clGreen);
 FillRect(73,5,1,1,clGreen); FillRect(78,5,1,1,clGreen);
 FillRect(73,9,1,1,clGreen); FillRect(78,9,1,1,clGreen);
 time_symbol_valid=true;
}
void parameters() {
 text(0,8,0,4,vs_labels[vsen],clYellow);
 text(1,44,0,3,coupling==CP_DC?"DC":coupling==CP_AC?"AC":"GND",clYellow);
 time_symbol();
 text(2,80,0,5,tb_labels[timebase],clGreen,focus==FC_Timebase);
 text(3,124,0,4,trigmode==TM_Auto?"AUTO":trigmode==TM_Normal?"NORM":"SING",clHotpink,focus==FC_TrigMode);
 edge_icon(); horizontal_indicator();
 generator_icon();
 status_badge(); measurement_icon(); cursor_icon(); debug_icon();
 if(!measurement_readout_valid || uint32_t(SystemMilliseconds-last_measurement_text_ms)>=250 || measured.valid!=measurement_readout.valid || measured.clipped!=measurement_readout.clipped) {
  measurement_readout=measured; measurement_readout_valid=true;
  last_measurement_text_ms=SystemMilliseconds;
 }
 char fields[4][11];
 if(measurement_panel || focus!=FC_Debug) {
  if(measurement_readout.valid && !measurement_readout.clipped) {
   dso::format_voltage(fields[0],"PP",measurement_readout.pp_uv);
   dso::format_voltage(fields[1],"R",measurement_readout.rms_uv);
  } else {
   const char* pp=measurement_readout.clipped?"PP CLIP":"PP--";
   const char* rms=measurement_readout.clipped?"R CLIP":"R--";
   for(unsigned i=0;(fields[0][i]=pp[i]);++i) {}
   for(unsigned i=0;(fields[1][i]=rms[i]);++i) {}
  }
  if(measurement_readout.timing_valid) {
   dso::format_frequency(fields[2],measurement_readout.frequency_uhz);
   dso::format_fixed(fields[3],"D",measurement_readout.duty_permille,1,"%");
  } else {
   const char* frequency=measurement_readout.too_fast?"F FAST":"F--";
   for(unsigned i=0;(fields[2][i]=frequency[i]);++i) {}
   fields[3][0]='D'; fields[3][1]='-'; fields[3][2]='-'; fields[3][3]=0;
  }
 } else {
  if(measurement_readout.valid) dso::format_fixed(fields[0],"A",(measurement_readout.mean_adc_q8*10+128)/256,1,"");
  else { fields[0][0]='A'; fields[0][1]='-'; fields[0][2]='-'; fields[0][3]=0; }
  dso::format_fixed(fields[1],voffset<0?"Z-":"Z+",voffset<0?-voffset:voffset,0,"");
  format_hpos(fields[2]);
  const char* version="v0.37";
  for(unsigned i=0;(fields[3][i]=version[i]);++i) {}
 }
 if(focus==FC_TrigMode) {
  const char* label="TRIG MODE";
  const char* value=trigmode==TM_Auto?"AUTO":trigmode==TM_Normal?"NORMAL":"SINGLE";
  for(unsigned i=0;(fields[0][i]=label[i]);++i) {}
  for(unsigned i=0;(fields[1][i]=value[i]);++i) {}
  // Keep the measured frequency and duty in the remaining fields.
 }
 if(focus==FC_HPos) {
  for(unsigned i=0;i<4;++i) fields[i][0]=0;
 } 
 if(focus==FC_Debug && measurement_panel) {
  char raw[7]; U16 color; PowerReadDisplay(raw,&color);
  for(unsigned i=0;i<7;++i) fields[0][i]=raw[i];
  fields[0][7]=0;
  dso::format_fixed(fields[1],"P",PowerDutyHundredths(),2,"%");
  const char* cpu="CPU 56MHz"; const char* adc="ADC 14MHz";
  for(unsigned i=0;(fields[2][i]=cpu[i]);++i) {}
  for(unsigned i=0;(fields[3][i]=adc[i]);++i) {}
 }
 if(focus==FC_Generator) {
  const char* label=generator_enabled?"SQ50% ON":"SQ50% OFF";
  for(unsigned i=0;(fields[0][i]=label[i]);++i) {}
  dso::format_fixed(fields[1],"F",generator_hz,0,"Hz");
  dso::format_frequency(fields[2],board::generator_timing(generator_hz).frequency_uhz); fields[2][0]='=';
  dso::format_fixed(fields[3],"P",PowerDutyHundredths(),2,"%");
 }
 if(focus==FC_Cursors) {
  const int64_t ns_per_pixel=dso::timebase_ns[timebase]/25;
  if(!cursor_voltage) {
   dso::compact_quantity(fields[0],"A",int64_t(cursor_x[0]-160)*ns_per_pixel,true);
   dso::compact_quantity(fields[1],"B",int64_t(cursor_x[1]-160)*ns_per_pixel,true);
   const uint64_t delta=uint64_t(cursor_x[1]>cursor_x[0]?cursor_x[1]-cursor_x[0]:cursor_x[0]-cursor_x[1])*ns_per_pixel;
   dso::compact_quantity(fields[2],"dT",delta,true);
   if(delta) dso::format_frequency(fields[3],1000000000000000ULL/delta);
   else { fields[3][0]='F'; fields[3][1]='-'; fields[3][2]='-'; fields[3][3]=0; }
  } else {
   const int a=cursor_voltage_uv(cursor_y[0]),b=cursor_voltage_uv(cursor_y[1]);
   dso::compact_quantity(fields[0],"A",a); dso::compact_quantity(fields[1],"B",b);
   dso::compact_quantity(fields[2],"dV",b-a); const char* mode="VOLT"; for(unsigned i=0;(fields[3][i]=mode[i]);++i) {}
  }
 }
 static int extra_layout=-1;
 const bool extra=focus==FC_Measurements;
 const int layout=extra?1:focus==FC_HPos?2:0;
 if(layout!=extra_layout) { FillRect(0,222,320,16,clBlack); for(unsigned i=6;i<15;++i) text_cache[i].valid=false; extra_layout=layout; }
 if(focus==FC_HPos) {
  char value[11]; format_hpos(value);
  text(6,0,222,10,"+/- MOVE");
  text(7,120,222,10,value,clWhite,true);
  text(8,240,222,10,"L.OK:CNTR");
  return;
 }
 if(extra) {
  char values[5][11];
  if(measurement_readout.valid && !measurement_readout.clipped) {
   dso::compact_quantity(values[0],"Mx",measurement_readout.maximum_uv);
   dso::compact_quantity(values[1],"Mn",measurement_readout.minimum_uv);
   dso::compact_quantity(values[2],"Av",measurement_readout.mean_uv);
  } else {
   const char* missing[3]={measurement_readout.clipped?"MxCLIP":"Mx--",measurement_readout.clipped?"MnCLIP":"Mn--",measurement_readout.clipped?"AvCLIP":"Av--"};
   for(unsigned j=0;j<3;++j) for(unsigned i=0;(values[j][i]=missing[j][i]);++i) {}
  }
  if(measurement_readout.timing_valid) {
   dso::compact_quantity(values[3],"PW",measurement_readout.high_ns,true);
   dso::compact_quantity(values[4],"T",measurement_readout.period_ns,true);
  } else { const char* missing[2]={"PW--","T--"}; for(unsigned j=0;j<2;++j) for(unsigned i=0;(values[3+j][i]=missing[j][i]);++i) {} }
  for(unsigned i=0;i<5;++i) text(10+i,i*64,222,8,values[i]);
  return;
 }
 for(unsigned i=0;i<4;++i) text(6+i,i*80,222,10,fields[i],clWhite,(focus==FC_Generator && i==1) || (focus==FC_HPos && i==0) || (focus==FC_Cursors && i==cursor_selected));
}
int raw_ycoord(int sample) {
 // Apply gain after zero calibration, then translate; ycoord clips separately.
 return 117-dso::vertical_pixels(sample-2048-voffset,vsen)-vpos;
}
int ycoord(int sample) { return clamp<int>(raw_ycoord(sample),17,216); }
// Debug levels overlay the waveform; equal levels share the row by color.
U16 level_color(int x,int y,int zy,int ty,U16 background) {
 if(y==zy) background=clGreen;
 if(y==ty && (ty!=zy || x>=160)) background=clHotpink;
 if(focus==FC_Cursors && x>=10 && x<310) {
  for(unsigned i=0;i<2;++i) if(cursor_voltage?y==cursor_y[i]:x==cursor_x[i]) background=i==cursor_selected?clOrange:clDeepskyblue;
 }
 return background;
}
void include_level(int y,int& first,int& last) {
 if(y<17 || y>216) return;
 if(y<first) first=y;
 if(y>last) last=y;
}
void marker_margin(bool right,int zy,int ty,bool levels_changed) {
 if(trace_valid && !levels_changed && old_marker_focus==focus) return;
 ui_dirty=true;
 const int current=clamp<int>(right?ty:zy,17,216);
 const int previous=clamp<int>(right?old_trigger_y:old_zero_y,17,216);
 int first=trace_valid?clamp<int>((current<previous?current:previous)-5,17,216):17;
 int last=trace_valid?clamp<int>((current>previous?current:previous)+5,17,216):216;
 if(levels_changed) {
  include_level(zy,first,last); include_level(ty,first,last);
  include_level(old_zero_y,first,last); include_level(old_trigger_y,first,last);
 }
 U16 pixels[200];
 for(int local=0;local<(right?9:10);++local) {
  const int x=right?310+local:local;
  const int radius=right?(local<=6?local/2:-1):(local>=3?(9-local)/2:-1);
  const U16 arrow_color=focus==(right?FC_TrigLvl:FC_VPos)?clAqua:(right?clHotpink:clGreen);
  for(int py=first;py<=last;++py) {
   const bool selected=focus==(right?FC_TrigLvl:FC_VPos);
   U16 color=selected && py>=current-4 && py<=current+4?clNavy:clBlack;
   if(selected && (py==current-4 || py==current+4 || local==0 || local==(right?8:9)) && py>=current-4 && py<=current+4) color=clAqua;
   color=level_color(x,py,zy,ty,color);
   if(radius>=0 && py>=current-radius && py<=current+radius) color=arrow_color;
   pixels[py-first]=color;
  }
  WriteColumn(x,first,pixels,last-first+1);
 }
}
void trace() {
 uint8_t clip_flags=0;
 const int zy=117-vpos,ty=raw_ycoord(threshold);
 const bool levels_changed=zy!=old_zero_y || ty!=old_trigger_y;
 const bool cursor_now=focus==FC_Cursors;
 const bool cursor_changed=cursor_now!=old_cursors || (cursor_now && (cursor_selected!=old_cursor_selected || cursor_voltage!=old_cursor_voltage || cursor_x[0]!=old_cursor_x[0] || cursor_x[1]!=old_cursor_x[1] || cursor_y[0]!=old_cursor_y[0] || cursor_y[1]!=old_cursor_y[1] || old_marker_focus!=focus));
 const bool reference_changed=(focus==FC_HPos)!=(old_marker_focus==FC_HPos);
 const int64_t step=dso::pixel_step_q16(timebase,sample_ticks);
 int64_t position=dso::viewport_start_q16(hpos,reclen,step,rolling);
 if(!rolling) position=clamp<int64_t>(position+display_alignment_q16,0,int64_t(reclen-1)*65536-299*step);
 int last_y=117;
 U16 pixels[200];
 for(int x=0;x<300;++x) {
  const int index=clamp<int>(int(position>>16),0,reclen-1);
  const int next=index+1<reclen?index+1:index;
  const int delta=int(display_samples[next])-display_samples[index];
  const int sample=display_peak?display_samples[index]:display_samples[index]+((delta*int(position&65535))>>16);
  const int y=ycoord(sample);
  if(x==0) last_y=y;
  const int high_y=display_peak?ycoord(display_high[index]):y;
  const int low_sample=sample,high_sample=display_peak?display_high[index]:sample;
  if(raw_ycoord(high_sample)<17) clip_flags|=1;
  if(raw_ycoord(low_sample)>216) clip_flags|=2;
  if(high_sample>=4093) clip_flags|=5;
  if(low_sample<=2) clip_flags|=10;
  const int top=display_peak?high_y:(y<last_y?y:last_y);
  const int bottom=display_peak?y:(y>last_y?y:last_y);
  const bool cursor_column=cursor_changed && ((old_cursors && !old_cursor_voltage && (10+x==old_cursor_x[0] || 10+x==old_cursor_x[1])) || (cursor_now && !cursor_voltage && (10+x==cursor_x[0] || 10+x==cursor_x[1])));
  const bool cursor_rows=cursor_changed && ((old_cursors && old_cursor_voltage) || (cursor_now && cursor_voltage));
  const bool redraw_reference=(x==150 && reference_changed) || cursor_column;
  const bool restore_clip=old_clip_flags && x>=285 && x<=297;
  if(!trace_valid || old_top[x]!=top || old_bottom[x]!=bottom || levels_changed || redraw_reference || cursor_rows || restore_clip) {
   int first=trace_valid?(top<old_top[x]?top:old_top[x]):17;
   int last=trace_valid?(bottom>old_bottom[x]?bottom:old_bottom[x]):216;
   if(redraw_reference || restore_clip) { first=17; last=216; }
   if(cursor_rows) {
    if(old_cursors && old_cursor_voltage) { include_level(old_cursor_y[0],first,last); include_level(old_cursor_y[1],first,last); }
    if(cursor_now && cursor_voltage) { include_level(cursor_y[0],first,last); include_level(cursor_y[1],first,last); }
   }
   if(levels_changed) {
    include_level(zy,first,last); include_level(ty,first,last);
    include_level(old_zero_y,first,last); include_level(old_trigger_y,first,last);
   }
   for(int py=first;py<=last;++py) {
    U16 color=(x%25==0 || (py-17)%25==0 || x==299 || py==216)?clDimgray:clBlack;
    if(x==150) color=focus==FC_HPos?clAqua:clGray2;
    if(py>=top && py<=bottom) color=clYellow;
    pixels[py-first]=level_color(10+x,py,zy,ty,color);
   }
   WriteColumn(10+x,first,pixels,last-first+1);
   ui_dirty=true;
   old_top[x]=top; old_bottom[x]=bottom;
  }
  last_y=y; position+=step;
 }
 marker_margin(false,zy,ty,levels_changed); marker_margin(true,zy,ty,levels_changed);
 if(clip_flags || old_clip_flags) ui_dirty=true;
 for(unsigned side=0;side<2;++side) if(clip_flags&(1U<<side)) {
  const U16 color=clip_flags&(4U<<side)?clRed:clOrange;
  for(int row=0;row<7;++row) FillRect(301-row,side?216-row:17+row,1+2*row,1,color);
 }
 old_clip_flags=clip_flags;
 rendered_timebase=timebase;
 old_cursors=cursor_now; old_cursor_voltage=cursor_voltage; old_cursor_selected=cursor_selected;
 for(unsigned i=0;i<2;++i) { old_cursor_x[i]=cursor_x[i]; old_cursor_y[i]=cursor_y[i]; }
 trace_valid=true; old_trigger_y=ty; old_zero_y=zy; old_marker_focus=focus;
}
}

U16* SampleBuf=display_samples;
U16* CurrentSample=display_samples;
U8 LibVersion[]="vibeDSO 0.37";
U8 NoScopePanel=0;

void DSO_Init(void) {
 Critical guard;
 RCC->AHBENR |= RCC_AHBENR_DMA1EN;
 RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
 RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
 TIM2->CR1=0;
 ADC1->CR1=0; ADC1->CR2=ADC_CR2_ADON;
 ADC1->SMPR1=0; ADC1->SMPR2=0;
 ADC1->SQR1=0; ADC1->SQR2=0; ADC1->SQR3=0; // PA0, ADC channel 0
 Delay(1000);
 ADC1->CR2 |= ADC_CR2_RSTCAL; while(ADC1->CR2&ADC_CR2_RSTCAL) {}
 ADC1->CR2 |= ADC_CR2_CAL; while(ADC1->CR2&ADC_CR2_CAL) {}
 for(unsigned i=0;i<1024;++i) ring[i]=display_samples[i]=2048;
 NVIC_SetPriority(DMA1_Channel1_IRQn,0);
 NVIC_SetPriority(ADC1_2_IRQn,0);
 NVIC_EnableIRQ(DMA1_Channel1_IRQn); NVIC_EnableIRQ(ADC1_2_IRQn);
 dirty=Disp_Panel|Disp_Param|Disp_Trace;
}
void StartCapture(void) {
 Critical guard;
 if(status&DSO_Hold) return;
 stop_hardware(); ready=false; triggered=false; post_count=filled=write_index=0;
 previous=2048; samples_seen=0;
 capture_alignment_q16=0;
 status=DSO_NormalDisplay;
 rolling=timebase<=TB_50ms;
 if(rolling) status|=DSO_Rolling;
 update_trigger_threshold(); rising=edge==TE_Rising;
 const dso::Timing requested=dso::timing(timebase); sample_ticks=requested.ticks;
 const dso::Timing t=rolling?dso::Timing{0,uint16_t(roll_raw_ticks-1),roll_raw_ticks,false}:requested;
 capture_sensitivity=vsen; capture_coupling=coupling;
 if(rolling) {
  const uint64_t window=dso::timebase_ns[timebase]*12/10000;
  roll_meter.reset(uint32_t(window>600000000?600000000:window));
  roll_phase=0; roll_min=4095; roll_max=0; roll_raw_write=roll_raw_count=0;
  for(unsigned i=0;i<1024;++i) ring[i]=roll_high[i]=2048+voffset;
 }

 if(t.continuous && display_peak) {
  // The fast DMA will reuse the envelope/analysis storage: invalidate it first.
  display_peak=false; have_measurement_frame=false; measurements_pending=true;
  dirty|=Disp_Trace|Disp_Param;
 }
 continuous_mode=t.continuous; dma_expected_second=false; dma_capture_end_total=0;
 if(!t.continuous) {
  TIM2->PSC=t.psc; TIM2->ARR=t.arr; TIM2->CNT=0;
  TIM2->CCMR1=0x6800; // OC2 PWM1, preload
  TIM2->CCR2=(uint32_t(t.arr)+1)/2;
  TIM2->CCER=TIM_CCER_CC2E;
  TIM2->EGR=TIM_EGR_UG; TIM2->SR=0;
 }
 dma_mode=rolling || t.continuous || sample_ticks<board::core_hz/20000;
 ADC1->SR=0;
 ADC1->CR1=dma_mode?0:ADC_CR1_EOCIE;
 // Power down is used to abort continuous conversion on HOLD/end-of-record.
 // Calibration is retained; allow >=1 us stabilization before restarting.
 ADC1->CR2=ADC_CR2_ADON;
#ifndef DSO_HOST_TEST
 for(volatile unsigned settle=0;settle<board::core_hz/1000000;++settle) {}
#endif
 DMA1->IFCR=0x0f;
 NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn); NVIC_ClearPendingIRQ(ADC1_2_IRQn);
 if(dma_mode) {
  DMA1_Channel1->CPAR=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&ADC1->DR));
  DMA1_Channel1->CMAR=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dma_samples));
  DMA1_Channel1->CNDTR=continuous_mode?dma_continuous_count:dma_timer_count;
  DMA1_Channel1->CCR=DMA_CCR_MINC|DMA_CCR_CIRC|DMA_CCR_PSIZE_0|DMA_CCR_MSIZE_0|
                        DMA_CCR_PL_1|DMA_CCR_HTIE|DMA_CCR_TCIE|DMA_CCR_TEIE|DMA_CCR_EN;
 }
 started_ms=SystemMilliseconds; active=true;
 if(t.continuous) {
  ADC1->CR2=ADC_CR2_ADON|ADC_CR2_CONT|ADC_CR2_DMA|ADC_CR2_EXTTRIG|(7U<<17);
  ADC1->CR2 |= ADC_CR2_SWSTART;
 } else {
  ADC1->CR2=ADC_CR2_ADON|ADC_CR2_EXTTRIG|(3U<<17)|(dma_mode?ADC_CR2_DMA:0);
  TIM2->CR1=TIM_CR1_CEN;
 }
}
void StopCapture(void) { Critical guard; stop_hardware(); }
void UpdateTimebase(void) { StopCapture(); StartCapture(); }
U16 GetDsoStatus(void) { return status; }
void SetHold(void) { Critical guard; status|=DSO_Hold; stop_hardware(); measurement_readout_valid=false; dirty|=Disp_Param; }
void ClrHold(void) { Critical guard; status&=~DSO_Hold; dirty|=Disp_Param; }
S8 SetVSen(S8 v) { const auto old=vsen; vsen=clamp<int>(v,VSenMin,VSenMax); if(active && old!=vsen) StartCapture(); measurements_pending=true; measurement_readout_valid=false; dirty|=Disp_Param|Disp_Trace; return vsen; }
S8 GetVSen(void) { return vsen; }
S8 SetCpl(S8 v) { const auto old=coupling; coupling=clamp<int>(v,CplMin,CplMax); if(active && old!=coupling) StartCapture(); dirty|=Disp_Param; return coupling; }
S8 GetCpl(void) { return coupling; }
S16 SetVPos(S16 v) { vpos=clamp<int>(v,VPosMin,VPosMax); dirty|=Disp_Trace; return vpos; }
S16 GetVPos(void) { return vpos; }
S8 SetTimeBase(S8 v) { timebase=clamp<int>(v,TBMin,TBMax); dirty|=Disp_Param; return timebase; }
S8 GetTimebase(void) { return timebase; }
S16 SetHPos(S16 v) { hpos=clamp<int>(v,0,reclen-300); dirty|=Disp_Trace; return hpos; }
S16 GetHPos(void) { return hpos; }
S8 SetTrigMode(S8 v) { trigmode=clamp<int>(v,TrigModeMin,TrigModeMax); return trigmode; }
S8 GetTrigMode(void) { return trigmode; }
S8 SetTrigEdge(S8 v) { Critical guard; edge=clamp<int>(v,TrigEdgeMin,TrigEdgeMax); rising=edge==TE_Rising; return edge; }
S8 GetTrigEdge(void) { return edge; }
S16 SetTrigLvl(S16 v) { Critical guard; triglevel=v; update_trigger_threshold(); return triglevel; }
S16 StepTrigLvlPixels(S16 pixels) {
 const int target=dso::vertical_pixels(triglevel,vsen)+pixels;
 return SetTrigLvl(dso::clamp<int>(dso::adc_delta_for_pixels(target,vsen),-2048-voffset,2047-voffset));
}
S16 GetTrigLvl(void) { return triglevel; }
U16 SetRecLen(U16 v) { StopCapture(); reclen=clamp<int>(v,RecLenMin,RecLenMax); SetHPos(hpos); return reclen; }
U16 GetRecLen(void) { return reclen; }
S16 SetVPosOfs(S16 v) { Critical guard; voffset=clamp<int>(v,-255,255); measurements_pending=true; measurement_readout_valid=false; update_trigger_threshold(); return voffset; }
S16 GetVPosOfs(void) { return voffset; }
S16 GetAverage(void) { return average; }
U8 SetFocus(U8 v) { focus=v<FC_Max?v:0; dirty|=Disp_Param|Disp_Trace; return focus; }
void StepCursor(S16 step) {
 if(cursor_voltage) cursor_y[cursor_selected]=clamp<int>(cursor_y[cursor_selected]-step,17,216);
 else cursor_x[cursor_selected]=clamp<int>(cursor_x[cursor_selected]+step,10,309);
 dirty|=Disp_Trace|Disp_Param;
}
void SelectCursor(void) { cursor_selected^=1; dirty|=Disp_Trace|Disp_Param; }
void ToggleCursorMode(void) { cursor_voltage=!cursor_voltage; dirty|=Disp_Trace|Disp_Param; }
S16 GetSignalMidpoint(void) {
 if(rolling) { Critical guard; return S16(roll_meter.midpoint()); }
 return have_measurement_frame && measured.valid && !measured.clipped?S16((int(measured.minimum)+measured.maximum+1)/2):S16(average);
}
int DsoReadMirrorView(dso::MirrorView* v) {
 if(!v || !trace_valid || !text_cache[0].valid || status_badge_cache<0) return 0;
 auto& f=v->frame;
 f.sequence=ui_sequence; f.published_ms=SystemMilliseconds;
 const uint32_t power_revision=PowerReadDisplay(v->power,&v->power_color); f.sequence+=power_revision;
 f.ticks=(dso::timebase_ns[rendered_timebase]/25)*(board::core_hz/1000000)*65536/1000;
 f.count=display_count; f.zero=2048+voffset; f.sensitivity=vsen; f.coupling=coupling; f.gain=dso::vertical_gain(vsen);
 f.flags=((status_badge_cache==1)?1:0)|((status_badge_cache==2)?2:0)|(display_peak?4:0)|8;
 v->top=old_top; v->bottom=old_bottom; v->zero_y=old_zero_y; v->trigger_y=old_trigger_y;
 v->focus=old_marker_focus; v->edge=(icon_edge<0?0:icon_edge)|((icon_focus>0)?2:0); v->generator=generator_icon_state<0?0:generator_icon_state;
 v->measurements=measurement_icon_cache>0; v->cursors=cursor_icon_cache>0; v->status=status_badge_cache; v->hp_selected=hp_selected;
 v->bar_left=160-int(int32_t(hp_cached)*150/(hp_length-300)); v->bar_right=v->bar_left+150;
 v->cursor_flags=(old_cursors?1:0)|(old_cursor_voltage?2:0); v->cursor_selected=old_cursor_selected;
 for(unsigned i=0;i<2;++i) { v->cursor_x[i]=old_cursor_x[i]; v->cursor_y[i]=old_cursor_y[i]; }
 v->reference_color=old_marker_focus==FC_HPos?clAqua:clGray2;
 v->clip_flags=old_clip_flags;
 v->text_count=0;
 for(unsigned i=0;i<15 && v->text_count<9;++i) {
  const auto& c=text_cache[i]; if(!c.valid) continue;
  auto& t=v->texts[v->text_count++]; t.x=c.x; t.y=c.y; t.width=c.box_width; t.color=c.color; t.selected=c.selected; t.length=c.length;
  for(unsigned j=0;j<c.length;++j) t.chars[j]=c.chars[j];
 }
 return 1;
}
void DsoReadControlState(dso::ControlState* s) {
 if(!s) return;
 s->timebase=timebase; s->mode=trigmode; s->edge=edge; s->focus=focus; s->sensitivity=vsen; s->coupling=coupling;
 s->flags=((status&DSO_Hold)?1:0)|(generator_enabled?2:0)|(measurement_panel?4:0)|(cursor_voltage?8:0)|(cursor_selected?16:0);
 s->vpos=vpos; s->hpos=hpos; s->trigger=triglevel; s->reclen=reclen; s->generator=generator_hz; s->zero=voffset;
 s->cursor_a=cursor_voltage?cursor_y[0]:cursor_x[0]; s->cursor_b=cursor_voltage?cursor_y[1]:cursor_x[1];
}
void DsoResetView(void) {
 measurement_panel=true; cursor_voltage=false; cursor_selected=0;
 cursor_x[0]=85; cursor_x[1]=235; cursor_y[0]=67; cursor_y[1]=167;
 focus=FC_Timebase; measurement_readout_valid=false;
 DsoSetGenerator(1000,1); dirty|=Disp_Panel|Disp_Param|Disp_Trace;
}
void DsoSetGenerator(uint32_t hz,int enabled) {
 generator_hz=clamp<uint32_t>(hz,1,100000); generator_enabled=enabled!=0;
 board::configure_generator(*TIM3,generator_hz,generator_enabled); dirty|=Disp_Param;
}
void DsoSetCursor(int voltage,unsigned selected,unsigned position) {
 cursor_voltage=voltage!=0; cursor_selected=selected?1:0;
 if(cursor_voltage) cursor_y[cursor_selected]=clamp<unsigned>(position,17,216);
 else cursor_x[cursor_selected]=clamp<unsigned>(position,10,309);
 dirty|=Disp_Trace|Disp_Param;
}
int DsoReadExportFrame(dso::ExportFrame* f) {
 if(!f || !frame_sequence || !have_measurement_frame) return 0;
 f->samples=display_samples; f->maximum=display_peak?display_high:nullptr;
 f->ticks=display_sample_ticks; f->sequence=frame_sequence; f->published_ms=frame_published_ms;
 f->count=display_count; f->flags=((frame_status&DSO_Trigged)?1:0)|((status&DSO_Hold)?2:0)|(display_peak?4:0)|8|(measured.clipped?32:0)|(last_frame_overrun?64:0)|(frame_partial_peak?128:0);
 f->zero=frame_zero; f->gain=frame_gain; f->sensitivity=frame_sensitivity; f->coupling=frame_coupling;
 return 1;
}
void StepGenerator(S16 direction) {
 generator_hz=clamp<int>(int(generator_hz)+direction,1,100000);
 board::configure_generator(*TIM3,generator_hz,generator_enabled);
 dirty|=Disp_Param;
}
void ToggleGenerator(void) {
 generator_enabled=!generator_enabled;
 board::configure_generator(*TIM3,generator_hz,generator_enabled);
 dirty|=Disp_Param;
}
U8 GetFocus(void) { return focus; }
void UpdateDisp(U16 flags) { dirty|=flags; }
void ToggleMeasurementPanel(void) { measurement_panel=!measurement_panel; measurement_readout_valid=false; dirty|=Disp_Param; }
void Rolling(void) { /* Streaming is handled in IRQs and DsoDisplay. */ }
void DsoTick(void) {
 Critical guard;
 if(trigger_badge_active && int32_t(SystemMilliseconds-trigger_badge_until)>=0) { trigger_badge_active=false; dirty|=Disp_Param; }
 if(trigger_led_enabled && trigger_led_active && int32_t(SystemMilliseconds-trigger_led_until)>=0) {
  GPIOA->BSRR=1U<<15;
  trigger_led_active=false;
 }
}
void DsoDisplay(void) {
 if(!painted) {
  painted=true; ClrScreen(); GPIOA->BSRR=1U<<15;
  trigger_led_enabled=true; // boot LedBlink has finished before the main loop
 }
 if(ready) {
  Critical guard;
  snapshot(); ready=false; status|=DSO_CaptureDone;
  if(status&DSO_Trigged) { trigger_badge_active=true; trigger_badge_until=SystemMilliseconds+300; }
  if(trigmode==TM_Single && (status&DSO_Trigged)) status|=DSO_Hold;
 }
 if(rolling && trigmode==TM_Auto && active && uint32_t(SystemMilliseconds-last_roll_ms)>=100) {
  { Critical guard; snapshot(); }
  last_roll_ms=SystemMilliseconds;
 }
 if(!active && !(status&(DSO_CaptureDone|DSO_Hold))) StartCapture();
 if(measurements_pending) {
  // Analysis is intentionally outside the snapshot's IRQ critical section.
  measured=have_measurement_frame?(display_peak
    ?dso::measure_stream(roll_meter_view,roll_raw_ticks,vsen,2048+voffset)
    :dso::measure(display_samples,display_count,display_sample_ticks,vsen,2048+voffset)):dso::Measurements{};
  measurements_pending=false; dirty|=Disp_Param;
 }
 if(NoScopePanel) return;
 if(dirty&Disp_Panel) { debug_icon_cache=-1; icon_edge=icon_focus=generator_icon_state=status_badge_cache=measurement_icon_cache=cursor_icon_cache=-1; time_symbol_valid=false; hp_selected=true; hp_cached=-1; trace_valid=false; for(auto& cache:text_cache) cache.valid=false; }
 if((dirty&(Disp_Panel|Disp_Trace)) && (!trace_valid || uint32_t(SystemMilliseconds-last_display_ms)>=20)) {
  trace(); last_display_ms=SystemMilliseconds; dirty&=~Disp_Trace;
 }
 static uint32_t telemetry_revision=0;
 char telemetry[7]; U16 telemetry_color;
 const uint32_t revision=PowerReadDisplay(telemetry,&telemetry_color);
 if(revision!=telemetry_revision) { telemetry_revision=revision; dirty|=Disp_Param; }
 if(dirty&(Disp_Panel|Disp_Param)) parameters();
 dirty&=Disp_Trace;
 if(ui_dirty) { ++ui_sequence; ui_dirty=false; }
}
extern "C" void DMA1_Channel1_IRQHandler(void) {
 const uint32_t flags=DMA1->ISR;
 DMA1->IFCR=flags&0x0f;
 if(!active) return;
 if((flags&DMA_ISR_TEIF1) || (flags&(DMA_ISR_HTIF1|DMA_ISR_TCIF1))==(DMA_ISR_HTIF1|DMA_ISR_TCIF1)) {
  reject_dma_frame(); return;
 }
 if(flags&(DMA_ISR_HTIF1|DMA_ISR_TCIF1)) {
  const bool second=(flags&DMA_ISR_TCIF1)!=0;
  if(!dma_half_available(second,DMA1_Channel1->CNDTR)) { reject_dma_frame(); return; }
  if(continuous_mode) {
   if(second!=dma_expected_second) { reject_dma_frame(); return; }
   dma_expected_second=!dma_expected_second;
   if(rising) process_direct_dma_half<true>(second?dma_continuous_count/2:0);
   else process_direct_dma_half<false>(second?dma_continuous_count/2:0);
  } else {
   if(rising) process_dma_half<true>(second?dma_timer_count/2:0);
   else process_dma_half<false>(second?dma_timer_count/2:0);
  }
  if(!active && !ready) return; // Already rejected by the direct-window guard.
  const uint32_t reused=second?DMA_ISR_TCIF1:DMA_ISR_HTIF1;
  if(!dma_half_available(second,DMA1_Channel1->CNDTR) || (DMA1->ISR&reused)) reject_dma_frame();
 }
}
extern "C" void ADC1_2_IRQHandler(void) {
 if(ADC1->SR&ADC_SR_EOC) accept(ADC1->DR);
}
extern "C" void TIM1_CC_IRQHandler(void) { TIM1->SR=0; }
