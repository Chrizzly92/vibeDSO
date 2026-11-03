// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
void BootloaderHoldOutputsOff(); // after GPIO init, before LCD startup
// Early startup only, after GPIO/LCD init, before EEPROM/USART/timers/ADC.
void BootloaderStartupWindow();
