// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "power.hpp"
#include "power_control.hpp"
#include "clock_profile.hpp"
#ifdef DSO_POWER_HOST_TEST
#include "power_host.hpp"
#endif

extern volatile uint32_t SystemMilliseconds;
namespace {
power::Controller controller;
char power_previous[7]={' ',' ',' ',' ',' ',' ',' '};
uint16_t power_previous_color=0; uint32_t power_display_revision=0;
volatile bool enabled=false;
bool pending=false;
unsigned wait_ticks=0,sample_count=0;
uint32_t sample_sum=0;
volatile uint16_t published_adc=0,published_duty=0;
volatile uint16_t published_fault_duty=0;
volatile uint16_t published_fault_adc=0;
volatile uint8_t published_fault=0,published_regulated=0,published_valid=0;
volatile uint16_t switch_sen1=0,switch_sen2=0,switch_cpl=0;
volatile uint32_t switch_sample_ms=0;
volatile bool switches_valid=false;
void publish() {
 TIM4->CCR4=board::timer4_compare(controller.duty());
 published_adc=controller.filtered_adc(); published_duty=controller.duty();
 published_fault_duty=controller.fault_duty();
 published_fault_adc=controller.raw_adc();
 published_fault=uint8_t(controller.fault());
 published_regulated=controller.regulated(); published_valid=controller.primed();
}
}

void PowerInit(void) {
 uint32_t saved=0;
#ifndef DSO_POWER_HOST_TEST
 __asm volatile("mrs %0, primask\ncpsid i":"=r"(saved)::"memory");
#endif
 enabled=false; TIM4->CCR4=0; controller.reset();
 GPIOA->CRL &= ~((0xfU<<24)|(0xfffU<<4)); // PA6 and PA1..3 analog inputs
 // One ADC2 owner: injected group [VMON, SEN1, SEN2, CPL]. KeyScan reads its
 // coherent cache, never switches the regular ADC mux concurrently.
 constexpr uint32_t samples=(7U<<18)|(7U<<3)|(7U<<6)|(7U<<9);
 ADC2->SMPR2=(ADC2->SMPR2&~samples)|samples; // 239.5 cycles on all four channels
 ADC2->JSQR=(3U<<20)|6U|(2U<<5)|(1U<<10)|(3U<<15);
 ADC2->CR1 |= ADC_CR1_SCAN; // required for all four injected ranks to run
 ADC2->CR2=(ADC2->CR2&~ADC_CR2_JEXTSEL)|ADC_CR2_JEXTSEL|ADC_CR2_JEXTTRIG;
 ADC2->SR=~uint32_t(ADC_SR_JEOC); // write-one preserves regular EOC
 TIM4->CCMR2 |= TIM_CCMR2_OC4PE; // update CCR4 on overflow, no pulse truncation
 sample_count=sample_sum=wait_ticks=0; pending=false;
 switches_valid=false;
 publish(); enabled=true;
#ifndef DSO_POWER_HOST_TEST
 __asm volatile("msr primask, %0"::"r"(saved):"memory");
#else
 (void)saved;
#endif
}

void PowerTick(void) {
 if(!enabled) return;
 if(pending) {
  if(!(ADC2->SR&ADC_SR_JEOC)) {
   if(++wait_ticks>=5) { controller.trip(power::Fault::adc_timeout); publish(); enabled=false; }
   return;
  }
  const uint16_t sample=uint16_t(ADC2->JDR1);
  switch_sen1=uint16_t(ADC2->JDR2); switch_sen2=uint16_t(ADC2->JDR3); switch_cpl=uint16_t(ADC2->JDR4);
  switch_sample_ms=SystemMilliseconds; switches_valid=true;
  ADC2->SR=~uint32_t(ADC_SR_JEOC);
  pending=false; wait_ticks=0;
  const uint16_t prior_duty=controller.duty();
  if(!controller.observe(sample)) { publish(); sample_sum=sample_count=0; }
  else { sample_sum+=sample;
   if(controller.duty()!=prior_duty) publish(); // fast load-release reduction, no 5-ms delay
   if(++sample_count==5) {
   controller.step(uint16_t((sample_sum+2)/5)); sample_count=sample_sum=0;
   publish();
   }
  }
 }
 ADC2->CR2 |= ADC_CR2_JSWSTART;
 pending=true;
}

int PowerReadSwitches(uint16_t *sen1,uint16_t *sen2,uint16_t *cpl) {
 uint32_t saved=0;
#ifndef DSO_POWER_HOST_TEST
 __asm volatile("mrs %0, primask\ncpsid i":"=r"(saved)::"memory");
#endif
 const bool valid=switches_valid && uint32_t(SystemMilliseconds-switch_sample_ms)<100;
 if(valid) { *sen1=switch_sen1; *sen2=switch_sen2; *cpl=switch_cpl; }
#ifndef DSO_POWER_HOST_TEST
 __asm volatile("msr primask, %0"::"r"(saved):"memory");
#else
 (void)saved;
#endif
 return valid?1:0;
}

void PowerDisplay(void) {
 static uint32_t last=0;


 if(uint32_t(SystemMilliseconds-last)<250) return;
 last=SystemMilliseconds;
 // Coherent telemetry snapshot; rendering never runs with interrupts masked.
 uint32_t saved=0;
#ifndef DSO_POWER_HOST_TEST
 __asm volatile("mrs %0, primask\ncpsid i":"=r"(saved)::"memory");
#endif
 const uint16_t adc=published_adc;
 const uint16_t fault_duty=published_fault_duty;
 const uint16_t fault_adc=published_fault_adc;
 const uint8_t fault=published_fault,valid=published_valid,regulated=published_regulated;
#ifndef DSO_POWER_HOST_TEST
 __asm volatile("msr primask, %0"::"r"(saved):"memory");
#else
 (void)saved;
#endif
 char label[8]="PWR ...";
 U16 color=clYellow;
 if(fault) {
  const char message[]="PWR E0 "; for(int i=0;i<7;++i) label[i]=message[i];
  label[5]=char('0'+fault); color=clRed;
  const unsigned phase=(SystemMilliseconds/1000)%4;
  if(phase==1 && valid) {
   int mv=int(power::magnitude_from_adc(adc)); if(mv<0) mv=0; if(mv>9990) mv=9990;
   const int cv=(mv+5)/10;
   label[0]='!'; label[1]='R'; label[2]='-'; label[3]=char('0'+cv/100);
   label[4]='.'; label[5]=char('0'+cv/10%10); label[6]=char('0'+cv%10);
  } else if(phase==2) {
   const unsigned pct=board::timer4_duty_hundredths(fault_duty);
   label[0]='D'; label[1]=char('0'+pct/1000); label[2]=char('0'+pct/100%10);
   label[3]='.'; label[4]=char('0'+pct/10%10); label[5]=char('0'+pct%10); label[6]='%';
  } else if(phase==3) {
   label[0]='A'; label[1]=char('0'+fault_adc/1000%10); label[2]=char('0'+fault_adc/100%10);
   label[3]=char('0'+fault_adc/10%10); label[4]=char('0'+fault_adc%10); label[5]=label[6]=' ';
  }
 }
 else if(valid) {
  int mv=int(power::magnitude_from_adc(adc)); if(mv<0) mv=0;
  const int cv=(mv+5)/10;
  label[0]=regulated?'=':'~'; label[1]='R'; label[2]='-'; label[3]=char('0'+cv/100);
  label[4]='.'; label[5]=char('0'+cv/10%10); label[6]=char('0'+cv%10);
  color=regulated?clGreen:clYellow;
 }
 bool changed=false;
 for(int i=0;i<7;++i) {
  if(label[i]!=power_previous[i] || color!=power_previous_color)
   {
#ifndef DSO_SOURCE_ENGINE
    PutcGenic(256+8*i,0,U8(label[i]),color,clBlack,&ASC8X16);
#endif
    changed=true;
   }
  power_previous[i]=label[i];
 }
 power_previous_color=color;
 if(changed) ++power_display_revision;
}

uint16_t PowerDutyHundredths(void) { return uint16_t(board::timer4_duty_hundredths(published_duty)); }

uint32_t PowerReadDisplay(char text[7],uint16_t* color) {
 for(unsigned i=0;i<7;++i) text[i]=power_previous[i];
 if(color) *color=power_previous_color;
 return power_display_revision;
}
