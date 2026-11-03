// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
// Application-owned flash interface; numeric results preserve the board ABI.
#pragma once
#include <stdint.h>
typedef enum { FLASH_BUSY=1, FLASH_ERROR_PG=2, FLASH_ERROR_WRP=3,
               FLASH_COMPLETE=4, FLASH_TIMEOUT=5 } FLASH_Status;
void FLASH_Unlock(void);
FLASH_Status FLASH_ErasePage(uint32_t address);
FLASH_Status FLASH_ProgramHalfWord(uint32_t address, uint16_t value);
