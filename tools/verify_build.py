# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Inspect ARM ELF layout, initial SP and vector bindings. Requires pyelftools."""
import argparse,json,struct
from pathlib import Path
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(); p.add_argument('--engine',choices=['source','legacy'],default='source'); a=p.parse_args()
path=ROOT/'build'/a.engine/'dso138.elf'
with path.open('rb') as f:
 elf=ELFFile(f); symbols={s.name:s for s in elf.get_section_by_name('.symtab').iter_symbols()}
 assert elf['e_machine']=='EM_ARM' and elf.elfclass==32
 assert not [s.name for s in symbols.values() if s.name and s['st_shndx']=='SHN_UNDEF' and s['st_info']['bind']!='STB_WEAK']
 vectors=elf.get_section_by_name('.isr_vector').data()
 assert struct.unpack_from('<I',vectors)[0]==0x20005000
 bound={}
 for index,name in [(1,'Reset_Handler'),(15,'SysTick_Handler'),(27,'DMA1_Channel1_IRQHandler'),(34,'ADC1_2_IRQHandler'),(43,'TIM1_CC_IRQHandler')]:
  value=struct.unpack_from('<I',vectors,index*4)[0]
  assert value&1 and value==(symbols[name]['st_value']|1),(name,hex(value))
  assert symbols[name]['st_info']['bind']!='STB_WEAK' or name=='Reset_Handler'
  bound[name]=hex(value)
 flash_end=0; ram_end=0
 for segment in elf.iter_segments():
  if segment['p_type']!='PT_LOAD': continue
  if 0x08000000<=segment['p_paddr']<0x08010000:
   flash_end=max(flash_end,segment['p_paddr']+segment['p_filesz'])
  if 0x20000000<=segment['p_vaddr']<0x20005000:
   ram_end=max(ram_end,segment['p_vaddr']+segment['p_memsz'])
 assert flash_end<=0x0800c000
 assert ram_end+2048<=0x20005000
 if a.engine=='source':
  maptext=(path.parent/'dso138.map').read_text()
  assert 'libdso138.a' not in maptext
 report={'engine':a.engine,'machine':'ARM32','flash_image_end':hex(flash_end),'ram_static_end':hex(ram_end),'eeprom_reserved':'0x0800c000..0x0800c7ff','stack_headroom_bytes':0x20005000-ram_end,'vectors':bound,'checks':'passed'}
(ROOT/'analysis'/f'verify-{a.engine}.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
