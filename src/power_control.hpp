// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "power_config.hpp"
namespace power {
enum class Fault : uint8_t { none, adc_range, overvoltage, undervoltage, adc_timeout };
// Bounded integrating controller. No hidden/unbounded integral accumulator.
// Positive ADC error means raw V- is not negative enough -> increase on-time.
class Controller {
 int32_t filtered_q8_=0;
 unsigned under_steps_=0,release_wait_=0;
 uint16_t duty_=0,raw_=0,fault_duty_=0;
 bool primed_=false,acquired_=false,load_release_=false;
 Fault fault_=Fault::none;
public:
 void reset() { *this=Controller{}; }
 void trip(Fault fault) { if(fault_==Fault::none) { fault_=fault; fault_duty_=duty_; } duty_=0; }
 bool observe(uint16_t adc,bool raw_sample=true) {
  if(fault_!=Fault::none) return false;
  raw_=adc;
  if(adc<128 || adc>4000) trip(Fault::adc_range);
  else if(adc<overvoltage_adc) trip(Fault::overvoltage);
  if(fault_==Fault::none && raw_sample) {
   if(release_wait_) --release_wait_;
   // React to a real sag without waiting for the low-pass filter to catch up.
   if(acquired_ && primed_ && adc>adc_from_magnitude(7000) && filtered_q8_<int32_t(adc)*256)
    filtered_q8_=int32_t(adc)*256;
   if(adc<load_release_adc && !release_wait_) {
    release_wait_=load_release_spacing;
    load_release_=true;
    unsigned cut=unsigned(target_adc-adc)*load_release_gain;


    if(cut<8) cut=8;
    if(cut>load_release_max_cut) cut=load_release_max_cut;
    duty_=uint16_t(cut>=duty_?0:duty_-cut);
    if(primed_ && filtered_q8_>int32_t(adc)*256) filtered_q8_=int32_t(adc)*256;
   } else if(adc>=target_adc) { load_release_=false; release_wait_=0; }
  }
  return fault_==Fault::none;
 }
 void step(uint16_t mean_adc) {
  if(!observe(mean_adc,false)) return;
  if(!primed_) { filtered_q8_=int32_t(mean_adc)*256; primed_=true; }
  else filtered_q8_+=(int32_t(mean_adc)*256-filtered_q8_)/4;
  const int error=int(filtered_adc())-target_adc;
  if(error>deadband_adc) {
   const unsigned limit=acquired_?undervoltage_steps:startup_undervoltage_steps;
   if(++under_steps_>=limit) { trip(Fault::undervoltage); return; }
  } else under_steps_=0;
  if(error>=-deadband_adc && error<=deadband_adc) { acquired_=true; return; }
  // Once regulation has been acquired, recover a deep load-step sag faster.
  // Startup remains soft; downward/overvoltage handling is unchanged.
  const bool recovering=acquired_ && error>int(adc_from_magnitude(7000)-target_adc);
  const int upward_limit=recovering?max_step*4:max_step;
  int delta=error/((recovering || (acquired_ && error>0))?error_divisor/4:error_divisor);
  if(!delta) delta=error>0?1:-1;
  if(delta>upward_limit) delta=upward_limit;
  if(delta<-max_step) delta=-max_step;
  if(load_release_ && delta>0) delta=0;
  int next=int(duty_)+delta;
  duty_=uint16_t(next<0?0:next>pwm_max?pwm_max:next);
 }
 uint16_t duty() const { return duty_; }
 uint16_t fault_duty() const { return fault_duty_; }
 uint16_t raw_adc() const { return raw_; }
 uint16_t filtered_adc() const { return uint16_t((filtered_q8_+128)/256); }
 bool primed() const { return primed_; }
 Fault fault() const { return fault_; }
 bool regulated() const {
  const int error=int(filtered_adc())-target_adc;
  return primed_ && fault_==Fault::none && error>=-deadband_adc && error<=deadband_adc;
 }
};
}

