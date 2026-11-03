// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

namespace dso {
// Gain trims in Q10: 1024 = unity. Enum order 5V,2V,1V,.5V,.2V,.1V,50mV,20mV,10mV.
// Values fitted from external DC references with tools/calibrate.py.
// Ranges missing from the supplied CSV keep their previous values.
// See the calibration report for provenance and coverage.
constexpr uint16_t vertical_gain_q10[] = {1065,1070,1076,1009,1011,1078,1078,1078,1078};
constexpr unsigned vertical_gain(unsigned sensitivity) {
 return sensitivity>=4 && sensitivity<=12 ? vertical_gain_q10[sensitivity-4] : 1024;
}
}
