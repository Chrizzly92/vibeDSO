# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Offline checks for pinned third-party files and distributable notices."""
import hashlib,json,pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
def check():
    manifest=json.loads((ROOT/'vendor/upstream-manifest.json').read_text())
    for item in manifest:
        path=ROOT/item['path']
        assert hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256'],path
        assert '/master/' not in item['url'],item['url']
    for name in ['LICENSE','NOTICE','LICENSE.md','NOTICE.md','vendor/JYE-LICENSE.txt',
                 'docs/firmware-licenses/newlib-4.5.0.txt','docs/firmware-licenses/libgloss-4.5.0.txt',
                 'docs/firmware-licenses/GPL-3.0.txt','docs/firmware-licenses/GCC-runtime-exception-3.1.txt',
                 'docs/pc-licenses/Python-3.12.txt','docs/pc-licenses/Tcl-Tk.txt',
                 'docs/pc-licenses/pyserial-3.5.txt','docs/pc-licenses/PyInstaller.txt','docs/pc-licenses/JYE-font.txt']:
        assert (ROOT/name).stat().st_size>100,name
    for name in ['LICENSE','vendor/cmsis-device-f1/License.md','vendor/cmsis-core/LICENSE.txt']:
        assert 'Version 2.0, January 2004' in (ROOT/name).read_text(),name
    assert 'https://github.com/Chrizzly92/vibeDSO' in (ROOT/'NOTICE').read_text()
    assert not (ROOT/'vendor/Libraries').exists(),'Unexpected vendor directory; use the pinned components'
    for name in ['src/Eeprom.cpp','src/Eeprom.h','src/stm32f10x_conf.h','src/stm32f10x_it.cpp',
                 'src/stm32f10x_it.h','src/startup_stm32f10x_md.S']:
        text=(ROOT/name).read_text()
        assert 'SPDX-License-Identifier: Apache-2.0' in text,name
        assert 'MCD Application Team' not in text and 'THE PRESENT FIRMWARE' not in text,name
    print(f'PASS {len(manifest)} unchanged pinned upstream files and required source/binary license notices')
if __name__=='__main__': check()
