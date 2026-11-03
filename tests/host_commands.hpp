// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "host_mmio.hpp"
inline U16 button_port=0xffff;
constexpr U16 FLASH_COMPLETE=4; // Actual vendor FLASH_Status enum; read success is 0.
#define PB_Port button_port
#define PB_Bits 0xf000
inline volatile U16 TimerKeyScan=0;
void OutputTLvl(void);
void HostDSOInit(void);
