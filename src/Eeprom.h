// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
// Persistent ABI/layout retained for existing DSO138 settings; new implementation.
#pragma once
#include <stdint.h>
#define PAGE_SIZE ((uint16_t)0x400)
#define EEPROM_START_ADDRESS ((uint32_t)0x0800c000)
#define PAGE0_BASE_ADDRESS EEPROM_START_ADDRESS
#define PAGE1_BASE_ADDRESS (EEPROM_START_ADDRESS+PAGE_SIZE)
#define NumbOfVar ((uint8_t)20)
extern uint16_t VirtAddVarTab[NumbOfVar];
enum { Addr_TimeBase=0, Addr_HPos, Addr_Vsen, Addr_Cpl, Addr_Vpos,
       Addr_VPosOfs, Addr_TrigMode, Addr_TrigEdge, Addr_TrigLvl,
       Addr_TrigPos, Addr_RecLen, Addr_SettingStatus };
uint16_t EE_Init(void);
uint16_t EE_ReadVariable(uint16_t address,uint16_t* value);
uint16_t EE_WriteVariable(uint16_t address,uint16_t value);
