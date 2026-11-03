// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
namespace power {
// Official JYE schematic: V- BEFORE U4 -- R41(10k) -- PA6/TP13 -- R42(1k) -- VDDA.
constexpr int32_t vdda_mv=3300;
constexpr int32_t divider_ratio=10; // R41 / R42
constexpr int32_t target_mv=8000;   // raw V- magnitude BEFORE 79L05; headroom for AV-=-5 V
constexpr uint16_t adc_from_magnitude(int32_t mv) {
 return uint16_t((4095LL*(divider_ratio*vdda_mv-mv)+(divider_ratio+1)*vdda_mv/2)/((divider_ratio+1)*vdda_mv));
}
constexpr int32_t magnitude_from_adc(uint16_t adc) {
 return divider_ratio*vdda_mv-int32_t((int64_t(adc)*(divider_ratio+1)*vdda_mv+2047)/4095);
}
constexpr uint16_t target_adc=adc_from_magnitude(target_mv);
constexpr uint16_t overvoltage_adc=adc_from_magnitude(9000);
constexpr uint16_t load_release_adc=adc_from_magnitude(8250);
constexpr unsigned load_release_spacing=2; // raw 1-ms samples: do not compound cuts every tick
constexpr unsigned load_release_gain=8; // proportional response to actual over-target error
constexpr unsigned load_release_max_cut=1024; // emergency downward-only correction, <=25 percentage points
constexpr uint16_t pwm_max=2662; // 64.99% logical command; extra load reserve, normalized to the timer period
constexpr int deadband_adc=6; // about +/-53 mV with nominal components
constexpr int max_step=8;     // at most 8/4096 duty per 5-ms control step
constexpr int error_divisor=64; // gentler approach to 8 V with only 1 V OV headroom
constexpr unsigned undervoltage_steps=600; // 3 seconds outside tolerance -> latch OFF
constexpr unsigned startup_undervoltage_steps=1000; // allow up to 5 s for initial soft-start
static_assert(target_adc>128 && target_adc<4000,"Verify AV- divider configuration");
static_assert(pwm_max<=2662,"Keep bounded inverter drive below 65 percent");
}
