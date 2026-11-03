// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#define DSO_POWER_HOST_TEST
#include "../src/power.cpp"
#include "selector.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>
int checks=0;
void check(bool ok,const char* what) { if(!ok) { printf("FAIL %s\n",what); exit(1); } ++checks; printf("PASS %s\n",what); }
int main() {
 using namespace power;
 check(target_adc==2820,"R41/R42 conversion targets raw V- at minus 8 V BEFORE U4");
 check(magnitude_from_adc(adc_from_magnitude(8000))>=7990 && magnitude_from_adc(adc_from_magnitude(8000))<=8010,"voltage conversion round-trip");
 Controller c;
 check(c.duty()==0,"inverter starts OFF");
 c.step(adc_from_magnitude(0));
 check(c.duty()>0 && c.duty()<=max_step,"soft start increases duty slowly for missing negative rail");
 for(int i=0;i<350;++i) c.step(adc_from_magnitude(0));
 check(c.duty()==2662 && c.duty()==pwm_max,"PWM offers reserve above 55 percent but remains below 65 percent");
 const auto ceiling=c.duty();
 for(int i=0;i<80;++i) c.step(target_adc);
 check(c.regulated() && c.duty()<=ceiling,"deadband holds duty without hidden windup");
 c.reset(); for(int i=0;i<30;++i) c.step(adc_from_magnitude(6500));
 const auto before=c.duty();
 for(int i=0;i<30;++i) c.step(adc_from_magnitude(8600));
 check(c.duty()<before && c.fault()==Fault::none,"too-negative output reduces duty");
 check(!c.observe(0) && c.duty()==0 && c.fault()==Fault::adc_range,"shorted-low ADC switches inverter OFF");
 c.step(target_adc); check(c.fault()==Fault::adc_range && c.duty()==0,"fault stays latched until reset");
 c.reset(); check(!c.observe(4095) && c.fault()==Fault::adc_range,"open R41 or shorted-high ADC latches OFF");
 c.reset(); check(!c.observe(adc_from_magnitude(9500)) && c.fault()==Fault::overvoltage,"raw overvoltage bypasses smoothing and switches OFF");
 c.reset(); for(unsigned i=0;i<startup_undervoltage_steps;++i) c.step(adc_from_magnitude(3000));
 check(c.fault()==Fault::undervoltage && c.duty()==0,"unreachable startup target stops after 5 seconds");
 c.reset(); c.step(target_adc);
 for(unsigned i=0;i<undervoltage_steps+20;++i) c.step(adc_from_magnitude(3000));
 check(c.fault()==Fault::undervoltage && c.duty()==0,"undervoltage after regulation still switches off after 3 seconds");
 // Behavioral plant family, not a model of the actual inductor/transistors.
 bool converged=true,bounded=true;
 const int slopes[]={9,12,16},taus[]={2,10,30};
 for(int slope:slopes) for(int tau:taus) {
  c.reset(); double magnitude=0;
  for(int step=0;step<3000;++step) {
   // Model raw converter voltage BEFORE the LDO, not its -5 V output.
   double equilibrium=double(c.duty())*slope;
   if(equilibrium>10000) equilibrium=10000;
   magnitude+=(equilibrium-magnitude)/tau;
   c.step(adc_from_magnitude(int32_t(magnitude)));
   if(c.duty()>pwm_max) bounded=false;
  }
  if(c.fault()!=Fault::none || !c.regulated() || magnitude<7900 || magnitude>8100) {
   printf("Plant slope=%d tau=%d: fault=%u raw=%.1f duty=%u\n",slope,tau,unsigned(c.fault()),magnitude,c.duty());
   converged=false;
  }
 }
 check(converged && bounded,"bounded feedback converges across nine delayed illustrative plants");
 c.reset(); double weak_raw=0; uint16_t max_used=0;
 for(int step=0;step<2000;++step) {
  weak_raw+=(double(c.duty())*4-weak_raw)/10;
  c.step(adc_from_magnitude(int32_t(weak_raw)));
  if(c.duty()>max_used) max_used=c.duty();
 }
 check(c.fault()==Fault::none && c.regulated() && max_used>1023 && max_used<=pwm_max,
       "weak USB plant converges to 8 V within reserved PWM range");
 c.reset(); weak_raw=0; max_used=0;
 for(int step=0;step<2000;++step) {
  weak_raw+=(double(c.duty())*3.7-weak_raw)/10;
  c.step(adc_from_magnitude(int32_t(weak_raw)));
  if(c.duty()>max_used) max_used=c.duty();
 }
 check(c.fault()==Fault::none && c.regulated() && max_used>2048 && max_used<=pwm_max,
       "added PWM reserve reaches 8 V in plant that cannot reach target at 50 percent");
 c.reset(); weak_raw=0; max_used=0;
 for(int step=0;step<2000;++step) {
  weak_raw+=(double(c.duty())*3.4-weak_raw)/10;
  c.step(adc_from_magnitude(int32_t(weak_raw)));
  if(c.duty()>max_used) max_used=c.duty();
 }
 check(c.fault()==Fault::none && c.regulated() && max_used>2252 && max_used<=pwm_max,
       "new reserve reaches 8 V in illustrative plant limited by old 54.98 percent ceiling");
 PowerInit();
 check(TIM4->CCR4==0 && TIM4->CCR3==2048,"initialization leaves trigger PWM unchanged and inverter OFF");
 check((GPIOA->CRL&((0xfU<<24)|(0xfffU<<4)))==0 && ADC2->JSQR==((3U<<20)|6U|(2U<<5)|(1U<<10)|(3U<<15)),"fixed injected group scans VMON, SEN1, SEN2, CPL in order");
 check((GPIOA->CRL&(0xfU<<20))==(0xfU<<20),"neighbor PA5 testpoint remains unchanged");
 check((ADC2->SMPR2&((7U<<18)|(7U<<3)|(7U<<6)|(7U<<9)))==((7U<<18)|(7U<<3)|(7U<<6)|(7U<<9)) && (ADC2->SMPR2&(7U<<15))==0,"all four ADC inputs settle at long sample time without touching PA5");
 check(ADC2->SQR3==3 && (ADC2->SR&2),"regular switch sequence preserved and EOC not cleared");
 check(TIM4->CCMR2&TIM_CCMR2_OC4PE,"CCR4 preload enabled for whole PWM pulses");
 check(ADC2->CR1&ADC_CR1_SCAN,"ADC2 scan mode explicitly enabled for multi-rank injected sequence");
 PowerTick();
 for(int i=0;i<5;++i) { ADC2->SR=4; ADC2->JDR1=adc_from_magnitude(0); PowerTick(); }
 check(TIM4->CCR4>0 && TIM4->CCR4<=max_step,"five injected samples produce one bounded control update");
 ADC2->SR=0; for(int i=0;i<5;++i) PowerTick();
 check(TIM4->CCR4==0 && published_fault==uint8_t(Fault::adc_timeout),"missing injected completion times out and switches OFF");
 PowerInit(); PowerTick(); ADC2->SR=4; ADC2->JDR1=0; PowerTick();
 check(TIM4->CCR4==0 && published_fault==uint8_t(Fault::adc_range),"individual invalid raw sample shuts down before averaging");
 PowerInit(); PowerTick();
 for(int i=0;i<5;++i) { ADC2->SR=4; ADC2->JDR1=target_adc; PowerTick(); }
 SystemMilliseconds=250; PowerDisplay();
 const char expected[]="=R-8.00"; bool correct_label=true;
 for(int i=0;i<7;++i) if(power_text[i]!=expected[i]) correct_label=false;
 check(correct_label,"telemetry explicitly labels raw minus 8 V rather than measured AV-");
 uint16_t s1=0,s2=0,cp=0;
 PowerInit(); check(!PowerReadSwitches(&s1,&s2,&cp),"switch cache unavailable before a complete injected group");
 PowerTick(); ADC2->SR=4; ADC2->JDR1=target_adc; ADC2->JDR2=1100; ADC2->JDR3=2500; ADC2->JDR4=1700; PowerTick();
 check(PowerReadSwitches(&s1,&s2,&cp) && s1==1100 && s2==2500 && cp==1700,"switch values come from their own injected result registers");
 controller.trip(Fault::undervoltage); publish();
 ADC2->SR=4; ADC2->JDR1=target_adc; ADC2->JDR2=1200; ADC2->JDR3=2600; ADC2->JDR4=1800; PowerTick();
 check(TIM4->CCR4==0 && PowerReadSwitches(&s1,&s2,&cp) && s1==1200 && s2==2600 && cp==1800,"selectors continue updating after inverter fault shutdown");
 SystemMilliseconds+=100;
 check(!PowerReadSwitches(&s1,&s2,&cp),"stale ADC groups cannot change switch indications");
 dso::Selector selector;
 check(selector.update(1200)==255 && selector.update(1200)==255 && selector.update(1200)==1,"three scans required before accepting initial selector position");
 bool stable=true;
 for(int i=0;i<60;++i) if(selector.update(i%2?740:790)!=1) stable=false;
 check(stable,"threshold chatter cannot toggle selector state");
 check(selector.update(0)==1 && selector.update(1200)==1,"one bad switch sample is ignored");
 selector.update(0); selector.update(0);
 check(selector.update(0)==0,"real selector movement accepted after three stable scans");
 c.reset(); for(unsigned i=0;i<startup_undervoltage_steps;++i) c.step(adc_from_magnitude(0));
 check(c.duty()==0 && c.fault_duty()==pwm_max,"shutdown preserves applied PWM for fault diagnostics");
 PowerInit(); for(unsigned i=0;i<startup_undervoltage_steps;++i) controller.step(adc_from_magnitude(0)); publish();
 SystemMilliseconds=8000; PowerDisplay();
 check(power_text[0]=='P' && power_text[4]=='E' && power_text[5]=='3',"fault telemetry retains E3 code");
 SystemMilliseconds=9000; PowerDisplay();
 check(power_text[0]=='!' && power_text[1]=='R',"fault telemetry shows frozen pre-shutdown raw voltage");
 SystemMilliseconds=10000; PowerDisplay();
 const char duty_label[]="D64.97%"; bool duty_label_ok=true;
 for(int i=0;i<7;++i) if(power_text[i]!=duty_label[i]) duty_label_ok=false;
 check(duty_label_ok,"fault telemetry reports applied duty before output was switched off");
 SystemMilliseconds=11000; PowerDisplay();
 const auto fault_code=controller.raw_adc();
 const unsigned displayed_code=unsigned(power_text[1]-'0')*1000+unsigned(power_text[2]-'0')*100+unsigned(power_text[3]-'0')*10+unsigned(power_text[4]-'0');
 check(power_text[0]=='A' && displayed_code==fault_code,"fault diagnostic preserves exact triggering ADC code");
 PowerInit(); PowerTick();
 const uint16_t pins[7]={0,3100,1200,400,0,0,target_adc};
 InjectedScan(pins); PowerTick();
 check(PowerReadSwitches(&s1,&s2,&cp) && s1==1200 && s2==3100 && cp==400,
       "ADC model decodes actual JSQR and SCAN configuration before producing switch results");
 const uint16_t levels[3]={0,1200,3200}; bool combinations_ok=true;
 for(uint8_t p1=0;p1<3;++p1) for(uint8_t p2=0;p2<3;++p2) {
  dso::Selector a,b;
  for(int scan=0;scan<3;++scan) {
   uint16_t input[7]={0,levels[p2],levels[p1],1200,0,0,target_adc};
   InjectedScan(input); PowerTick(); PowerReadSwitches(&s1,&s2,&cp);
   const auto va=a.update(s1),vb=b.update(s2);
   if(scan==2 && (va!=p1 || vb!=p2 || dso::sensitivity_code(va,vb)!=4+3*p1+p2)) combinations_ok=false;
  }
 }
 check(combinations_ok,"all nine SEN1/SEN2 combinations propagate from scanned pins to sensitivity code");
 c.reset(); for(int i=0;i<200;++i) c.step(adc_from_magnitude(0));
 const auto driven=c.duty();
 check(c.observe(adc_from_magnitude(8500)) && c.duty()<driven && c.fault()==Fault::none,
       "raw over-target voltage immediately reduces duty before hard E2 threshold");
 const auto reduced=c.duty(); c.step(adc_from_magnitude(7000));
 check(c.duty()<=reduced,"filtered old undervoltage cannot immediately undo load-release PWM cut");
 PowerInit(); for(int i=0;i<200;++i) controller.step(adc_from_magnitude(0)); publish(); PowerTick();
 const auto hardware_before=TIM4->CCR4;
 ADC2->SR=ADC_SR_JEOC; ADC2->JDR1=adc_from_magnitude(8500); PowerTick();
 check(TIM4->CCR4<hardware_before && published_fault==0,
       "first raw sample applies fast PWM reduction without waiting for five-sample mean");
 bool release_ok=true;
 const int response_ms[]={12,30,80};
 for(int tau:response_ms) {
  c.reset(); double voltage=0,peak=0; unsigned sum=0;
  for(int ms=0;ms<12000;++ms) {
   const double gain=ms<6000?3.4:6.0;
   voltage+=(double(c.duty())*gain-voltage)/tau;
   if(ms>=6000 && voltage>peak) peak=voltage;
   const auto code=adc_from_magnitude(int32_t(voltage)); c.observe(code); sum+=code;
   if(ms%5==4) { c.step(uint16_t((sum+2)/5)); sum=0; }
  }
  if(c.fault()!=Fault::none || !c.regulated() || peak>=9000) {
   printf("Load release tau=%d: fault=%u peak=%.1f voltage=%.1f\n",tau,unsigned(c.fault()),peak,voltage);
   release_ok=false;
  }
 }
 check(release_ok,"three illustrative load-release plants settle without lifting 9 V E2 threshold");
 check(board::timer4_compare(pwm_max)==2070 && board::timer4_period==3186,"56 MHz inverter maps its logical limit to below 65 percent physical duty");
 check(TIM4->CCR4==board::timer4_compare(published_duty),"published hardware inverter compare follows normalized logical control output");

 bool cycling_ok=true;
 for(int tau:response_ms) for(int hz:{1,2,5,10}) {
  c.reset(); double voltage=0,minimum=10000,maximum=0; unsigned sum=0;
  for(int ms=0;ms<14000;++ms) {
   const double gain=ms<6000?4.6:((ms-6000)%(1000/hz)<500/hz?3.4:4.6);
   voltage+=(double(c.duty())*gain-voltage)/tau;
   if(ms>=6000) { if(voltage<minimum) minimum=voltage; if(voltage>maximum) maximum=voltage; }
   auto code=adc_from_magnitude(int32_t(voltage)); c.observe(code); sum+=code;
   if(ms%5==4) { c.step(uint16_t((sum+2)/5)); sum=0; }
  }
  printf("CYCLING tau=%d hz=%d min=%.0f max=%.0f fault=%u\n",tau,hz,minimum,maximum,unsigned(c.fault()));
  if(c.fault()!=Fault::none || minimum<5000 || maximum>=9000) cycling_ok=false;
 }
 check(cycling_ok,"twelve illustrative periodic load plants retain more than 5 V without lifting hard overvoltage protection");
 c.reset(); for(int i=0;i<200;++i) c.step(adc_from_magnitude(0));
 const unsigned before_small=c.duty(); c.observe(adc_from_magnitude(8300));
 const unsigned after_small=c.duty(); c.observe(adc_from_magnitude(8300));
 check(before_small-after_small<before_small/4 && c.duty()==after_small,"small overshoot has proportional cut and cannot repeat on the immediately following raw sample");

 c.reset(); double loaded=0;
 for(int ms=0;ms<9000;++ms) {
  loaded+=(double(c.duty())*3.1-loaded)/30;
  auto code=adc_from_magnitude(int32_t(loaded)); c.observe(code);
  if(ms%5==4) c.step(code);
 }
 check(c.fault()==Fault::none && c.regulated() && c.duty()>2457 && c.duty()<=2662,
       "extra PWM reserve reaches target in a model that needs more than the old 60 percent ceiling");
 c.reset(); for(int i=0;i<120;++i) c.step(adc_from_magnitude(0));
 for(int i=0;i<120;++i) c.step(target_adc);
 const auto before_sag=c.filtered_adc(); c.observe(adc_from_magnitude(6000));
 check(c.filtered_adc()>before_sag && c.filtered_adc()==adc_from_magnitude(6000),
       "acquired regulator observes a deep raw sag immediately instead of waiting for the low-pass filter");

 check(PowerDutyHundredths()==board::timer4_duty_hundredths(published_duty),"UI telemetry exports the applied inverter duty rather than test-generator duty");
 printf("%d power-control checks passed (simulated registers/illustrative plant only).\n",checks);
}

