// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "Eeprom.h"
#include "stm32f10x_flash.h"
#include "settings_store.hpp"
uint16_t VirtAddVarTab[NumbOfVar]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
namespace {
struct SettingsFlash {
 uint16_t read(unsigned offset) const {
  return *reinterpret_cast<volatile const uint16_t*>(EEPROM_START_ADDRESS+offset);
 }
 uint16_t program(unsigned offset,uint16_t value) {
  return FLASH_ProgramHalfWord(EEPROM_START_ADDRESS+offset,value);
 }
 uint16_t erase(unsigned offset) { return FLASH_ErasePage(EEPROM_START_ADDRESS+offset); }
} flash;
settings::Store<SettingsFlash> store(flash);
}
uint16_t EE_Init(void) { return store.init(); }
uint16_t EE_ReadVariable(uint16_t address,uint16_t* value) { return store.read(address,value); }
uint16_t EE_WriteVariable(uint16_t address,uint16_t value) { return store.write(address,value); }
