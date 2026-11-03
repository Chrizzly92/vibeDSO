// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#pragma once
// The original archive and the vector table use unmangled C symbol names.
// Application implementations are compiled as C++17, with this public C ABI.
extern "C" {
#include "stm32f10x.h"
#include "stm32f10x_conf.h"
#include "Common.h"
#include "Board.h"
#include "Screen.h"
#include "Command.h"
#include "Eeprom.h"
#include "libdso138.h"
#include "stm32f10x_it.h"
}
