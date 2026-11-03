// Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
#include "stm32f10x_conf.h"
#include "Eeprom.h"
namespace {
FLASH_Status wait_flash() {
 uint32_t remaining=4000000;
 while((FLASH->SR & FLASH_SR_BSY) && --remaining) {}
 if(!remaining) return FLASH_TIMEOUT;
 if(FLASH->SR & FLASH_SR_WRPRTERR) return FLASH_ERROR_WRP;
 if(FLASH->SR & FLASH_SR_PGERR) return FLASH_ERROR_PG;
 return FLASH_COMPLETE;
}
bool settings_address(uint32_t a) {
 return a>=EEPROM_START_ADDRESS && a<EEPROM_START_ADDRESS+2*PAGE_SIZE && !(a&1);
}
}
void FLASH_Unlock(void) {
 if(FLASH->CR & FLASH_CR_LOCK) {
  FLASH->KEYR=0x45670123;
  FLASH->KEYR=0xcdef89ab;
 }
}
FLASH_Status FLASH_ErasePage(uint32_t address) {
 if(!settings_address(address) || (address & (PAGE_SIZE-1))) return FLASH_ERROR_PG;
 if(FLASH->CR & FLASH_CR_LOCK) return FLASH_ERROR_WRP;
 auto status=wait_flash();
 if(status==FLASH_TIMEOUT) return status;
 FLASH->SR=FLASH_SR_EOP|FLASH_SR_PGERR|FLASH_SR_WRPRTERR;
 FLASH->CR|=FLASH_CR_PER;
 FLASH->AR=address;
 FLASH->CR|=FLASH_CR_STRT;
 status=wait_flash();
 FLASH->CR&=~FLASH_CR_PER;
 return status;
}
FLASH_Status FLASH_ProgramHalfWord(uint32_t address,uint16_t value) {
 if(!settings_address(address)) return FLASH_ERROR_PG;
 if(FLASH->CR & FLASH_CR_LOCK) return FLASH_ERROR_WRP;
 auto status=wait_flash();
 if(status==FLASH_TIMEOUT) return status;
 FLASH->SR=FLASH_SR_EOP|FLASH_SR_PGERR|FLASH_SR_WRPRTERR;
 FLASH->CR|=FLASH_CR_PG;
 *reinterpret_cast<volatile uint16_t*>(address)=value;
 status=wait_flash();
 FLASH->CR&=~FLASH_CR_PG;
 if(status==FLASH_COMPLETE && *reinterpret_cast<volatile uint16_t*>(address)!=value)
  return FLASH_ERROR_PG;
 return status;
}
