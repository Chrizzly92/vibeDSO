# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Run the real engine.cpp against register stubs, using a native C++17 compiler."""
import json, os, pathlib, shutil, subprocess, sys
import argparse
parser=argparse.ArgumentParser(); group=parser.add_mutually_exclusive_group(); group.add_argument('--settings',action='store_true'); group.add_argument('--bootloader',action='store_true'); group.add_argument('--remote',action='store_true'); group.add_argument('--mirror',action='store_true'); group.add_argument('--stream',action='store_true'); group.add_argument('--link',action='store_true'); group.add_argument('--screen',action='store_true'); group.add_argument('--power',action='store_true'); group.add_argument('--commands',action='store_true'); group.add_argument('--measurements',action='store_true'); group.add_argument('--generator',action='store_true'); group.add_argument('--clocks',action='store_true'); parser.add_argument('--legacy-profile',action='store_true'); args=parser.parse_args()
if args.legacy_profile and not (args.clocks or args.generator): parser.error('--legacy-profile requires --clocks or --generator')
ROOT=pathlib.Path(__file__).resolve().parents[1]
(ROOT/'analysis').mkdir(exist_ok=True)
out=ROOT/'build/host-tests'; out.mkdir(parents=True,exist_ok=True)
stem='test_settings' if args.settings else 'test_bootloader' if args.bootloader else 'test_remote' if args.remote else 'test_mirror' if args.mirror else 'test_stream' if args.stream else 'test_link' if args.link else 'test_clock' if args.clocks else 'test_generator' if args.generator else 'test_measurements' if args.measurements else 'test_commands' if args.commands else 'test_power' if args.power else 'test_screen' if args.screen else 'test_engine'
source=ROOT/'tests'/(stem+'.cpp')
env=os.environ.copy()
compiler=shutil.which('g++') or shutil.which('clang++')
suffix='_legacy' if args.legacy_profile else ''
exe=out/(stem+suffix+('.exe' if os.name=='nt' else ''))
if compiler:
 command=[compiler,'-std=c++17','-O2','-Wall','-Wextra','-I'+str(ROOT/'src'),'-I'+str(ROOT/'tests'),str(source),'-o',str(exe)]
else:
 compiler=shutil.which('cl')
 if not compiler and os.name=='nt':
  vs=pathlib.Path(os.environ.get('ProgramFiles','C:/Program Files'))/'Microsoft Visual Studio'
  candidates=sorted(vs.glob('*/**/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))
  if candidates:
   compiler=str(candidates[-1]); tools=candidates[-1].parents[3]
   kits=pathlib.Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Windows Kits/10'
   sdk=sorted((kits/'Include').glob('10.*'))[-1].name
   env['INCLUDE']=str(tools/'include')+';'+str(kits/'Include'/sdk/'ucrt')
   env['LIB']=';'.join(str(p) for p in [tools/'lib/x64',kits/'Lib'/sdk/'ucrt/x64',kits/'Lib'/sdk/'um/x64'])
 if not compiler: sys.exit('Native C++ compiler missing (GCC, Clang or Visual Studio).')
 command=[compiler,'/nologo','/std:c++17','/EHsc','/O2','/W3','/MD','/I'+str(ROOT/'src'),'/I'+str(ROOT/'tests'),str(source),'/Fe:'+str(exe),'/Fo:'+str(out/(stem+suffix+'.obj'))]
if args.legacy_profile: command.insert(1, '-DDSO_LEGACY_ENGINE=1' if pathlib.Path(compiler).stem.lower() in ('g++','clang++') else '/DDSO_LEGACY_ENGINE=1')
r=subprocess.run(command,env=env,capture_output=True,text=True)
(out/(stem+suffix+'-compile.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
if r.returncode: print(r.stdout,r.stderr); sys.exit(r.returncode)
r=subprocess.run([str(exe)],capture_output=True,text=True,cwd=ROOT)
print(r.stdout)
(out/(stem+suffix+'-results.txt')).write_text(r.stdout+r.stderr,encoding='utf-8')
checks=[line[5:] for line in r.stdout.splitlines() if line.startswith('PASS ')]
scope='Actual Screen.cpp compiled natively against an ILI9341 command/pixel memory model.' if args.screen else 'Actual engine.cpp compiled natively with register stubs and framebuffer. No ADC, DMA timing, startup or electrical device simulation.'
if args.settings: scope='New settings store with a halfword flash model, legacy layout fixtures, reset at every transfer boundary and partial source erase. Physical brownout behavior is not simulated.'
if args.bootloader: scope='Boot chord, ROM vector guards and shared hardware handoff register model. ARM tail jump inspected in linked disassembly; actual ROM execution requires hardware validation.'
if args.remote: scope='Actual Command.cpp and engine.cpp with real wire serializer, settings flash status/failure model, reset chord and remote commands; no physical serial or electrical validation.'
if args.mirror: scope='Actual engine.cpp incremental display and original LCD font produce compact packets plus physical-shifted rasters for Python pixel comparison; no hardware UART or GUI timing.'
if args.stream: scope='Streaming integer accumulators and edge history with synthetic samples; no hardware cadence validation.'
if args.link: scope='Actual UART DMA/RX logic with register model, command parser, frozen packet and CRC tests; no physical serial transfer.'
if args.power: scope='Actual power.cpp/control compiled natively with injected-ADC register stubs and nine illustrative delayed plants; no real switching-power-stage validation.'
if args.commands: scope='Actual Command.cpp plus engine.cpp; physical button scans and EEPROM/register stubs. Peripheral startup replaced by a stub; no device timing simulation.'
if args.measurements: scope='Integer measurement algorithms and compact formatting, tested against synthetic analytic signals; no hardware bandwidth or accuracy validation.'
if args.generator: scope='Shared TIM3 initializer and counter/output-mode cycle model feeding actual measurement code; not a physical pin measurement.'
if args.clocks: scope='Shared clock startup with readiness and PLL write-protection model; SysTick, ADC/timer periods and normalized PWM. No physical oscillator or throughput measurements.'
report={'passed':len(checks),'exit_code':r.returncode,'checks':checks,'scope':scope,'profile':'legacy' if args.legacy_profile else 'source'}
(ROOT/'analysis'/('settings-tests.json' if args.settings else 'bootloader-tests.json' if args.bootloader else 'remote-tests.json' if args.remote else 'mirror-tests.json' if args.mirror else 'stream-tests.json' if args.stream else 'link-tests.json' if args.link else ('clock-tests-legacy.json' if args.legacy_profile else 'clock-tests.json') if args.clocks else ('generator-tests-legacy.json' if args.legacy_profile else 'generator-tests.json') if args.generator else 'measurement-tests.json' if args.measurements else 'command-tests.json' if args.commands else 'power-tests.json' if args.power else 'screen-tests.json' if args.screen else 'host-tests.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
sys.exit(r.returncode)
