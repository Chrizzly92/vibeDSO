# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Dependency-free cross build. Python 3.9+ and GNU Arm Embedded GCC required."""
import argparse, os, pathlib, shutil, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--toolchain',help='directory containing arm-none-eabi-g++')
    p.add_argument('--engine',choices=['source','legacy'],default='source')
    p.add_argument('--display-test',action='store_true',help='build static panel test (no capture)')
    a=p.parse_args()
    if a.engine=='legacy' and not (ROOT/'vendor/libdso138.a').exists():
        sys.exit('The optional legacy reference library is not included. Use --engine source.')
    bindir = a.toolchain or os.environ.get('ARM_GCC_BIN')
    if bindir: bindir = str(pathlib.Path(bindir).resolve())
    def tool(name):
        executable='arm-none-eabi-'+name+('.exe' if os.name=='nt' else '')
        found=str(pathlib.Path(bindir)/executable) if bindir else shutil.which(executable)
        if not found: sys.exit(f'Missing {executable}. Set ARM_GCC_BIN or --toolchain.')
        return found
    out=ROOT/'build'/(a.engine+('-display-test' if a.display_test else '')); out.mkdir(parents=True,exist_ok=True)
    vendor=ROOT/'vendor'
    includes=[ROOT/'src',vendor/'cmsis-core/Include',vendor/'cmsis-device-f1/Include']
    flags=['-mcpu=cortex-m3','-mthumb','-Os','-g3','-ffunction-sections','-fdata-sections','-fno-common','-DSTM32F10X_MD','-DHSE_VALUE=8000000UL','-DSTM32F103xB=1','-Wall','-Wextra']
    flags += ['-I'+str(i) for i in includes]
    if a.engine=='source': flags += ['-DDSO_SOURCE_ENGINE=1']
    else: flags += ['-DDSO_LEGACY_ENGINE=1']
    if a.display_test: flags += ['-DDSO_DISPLAY_TEST=1']
    cpp=['-std=gnu++17','-fno-exceptions','-fno-rtti','-fno-threadsafe-statics','-fno-use-cxa-atexit','-include',str(ROOT/'src/abi.hpp')]
    sources=list(sorted((ROOT/'src').glob('*.cpp')))
    if a.engine=='legacy': sources=[s for s in sources if s.name!='engine.cpp']
    sources += [vendor/'cmsis-device-f1/Source/Templates/system_stm32f1xx.c',ROOT/'src/startup_stm32f10x_md.S']
    objects=[]
    def run(args,log=False):
        r=subprocess.run([str(x) for x in args],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        if log: (out/'build.log').open('a',encoding='utf-8').write(r.stdout)
        if r.returncode:
            print(r.stdout); sys.exit(r.returncode)
        return r.stdout
    (out/'build.log').write_text('',encoding='utf-8')
    for s in sources:
        obj=out/(s.stem+'.o'); objects.append(obj)
        print('Compile',s.name,flush=True)
        run([tool('g++' if s.suffix=='.cpp' else 'gcc'),*flags,*(cpp if s.suffix=='.cpp' else ['-std=gnu99'] if s.suffix=='.c' else []),'-c',s,'-o',obj],True)
    elf=out/'dso138.elf'
    link=[tool('g++'),'-mcpu=cortex-m3','-mthumb','-nostartfiles','--specs=nano.specs','--specs=nosys.specs','-Wl,--gc-sections,--print-memory-usage','-Wl,-Map='+str(out/'dso138.map'),'-T',ROOT/'stm32f103.ld',*objects]
    if a.engine=='legacy': link.append(ROOT/'vendor/libdso138.a')
    link += ['-Wl,--start-group','-lc','-lm','-lgcc','-Wl,--end-group','-o',elf]
    print(run(link,True))
    for fmt,ext in [('ihex','hex'),('binary','bin')]: run([tool('objcopy'),'-O',fmt,elf,out/f'dso138.{ext}'])
    (out/'dso138.lst').write_text(run([tool('objdump'),'-d','-S',elf]),encoding='utf-8')
    (out/'symbols.txt').write_text(run([tool('nm'),'-n','-C',elf]),encoding='utf-8')
    print(run([tool('size'),elf]))
    (out/'toolchain.txt').write_text(run([tool('g++'),'--version']),encoding='utf-8')
    print('Built',elf)

if __name__=='__main__': main()
