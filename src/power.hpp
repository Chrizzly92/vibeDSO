// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
extern "C" {
void PowerInit(void);
void PowerTick(void);    // 1-ms SysTick, nonblocking injected ADC2 conversion
uint32_t PowerReadDisplay(char text[7],uint16_t* color);
uint16_t PowerDutyHundredths(void);
void PowerDisplay(void); // main loop only, cached 4-Hz status field
int PowerReadSwitches(uint16_t *sen1,uint16_t *sen2,uint16_t *cpl);
}
