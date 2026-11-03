# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Build the standalone Windows application. Requires pyinstaller and pyserial."""
import argparse,pathlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=pathlib.Path,default=ROOT/'build/desktop');args=parser.parse_args()
if sys.platform!='win32':parser.error('Build the Windows EXE on Windows.')
subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--onefile','--windowed','--name','vibeDSO-Desktop-v0.37',
 '--distpath',str(args.output.resolve()),'--workpath',str(ROOT/'build/pc-work'),'--specpath',str(ROOT/'build'),
 '--add-data',str(ROOT/'tools/dso138_font.bin')+':.',
 '--add-data',str(ROOT/'tools/demo/mirror-3.bin')+':demo',
 '--add-data',str(ROOT/'LICENSE')+':notices',
 '--add-data',str(ROOT/'NOTICE')+':notices',
 '--add-data',str(ROOT/'docs/pc-licenses')+':notices/pc-licenses',
 '--exclude-module','matplotlib',str(ROOT/'tools/dso138_app.py')],check=True)
