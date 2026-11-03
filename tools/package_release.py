# Copyright 2026 Chrizzly92. SPDX-License-Identifier: Apache-2.0
"""Package current source and binaries with their attribution/license materials."""
import argparse,hashlib,json,pathlib,re,subprocess,zipfile
from check_licenses import check
ROOT=pathlib.Path(__file__).resolve().parents[1]
COMMON=['LICENSE','NOTICE','LICENSE.md','NOTICE.md','vendor/JYE-LICENSE.txt',
        'vendor/cmsis-core/LICENSE.txt','vendor/cmsis-device-f1/License.md','vendor/upstream-manifest.json']
def notice_files():
    return [ROOT/p for p in COMMON]+[p for folder in ['docs/firmware-licenses','docs/pc-licenses'] for p in (ROOT/folder).rglob('*') if p.is_file()]
def archive(path,entries):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
        for source,name in sorted(entries,key=lambda pair:pair[1]):z.write(source,name)
    print(path)
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--desktop',type=pathlib.Path)
    parser.add_argument('--output',type=pathlib.Path,default=ROOT/'dist/releases')
    args=parser.parse_args();check()
    version=re.search(r'version="([^"]+)"',(ROOT/'src/branding.hpp').read_text())[1]
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    # Git index plus non-ignored new files; no .git directory or deleted files.
    names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=ROOT).decode().split('\0')
    files=[ROOT/n for n in sorted(set(names)) if n and (ROOT/n).is_file()]
    assert all('Libraries' not in p.relative_to(ROOT).parts for p in files)
    source=out/f'vibeDSO-v{version}-source.zip'
    archive(source,[(p,'vibeDSO/'+p.relative_to(ROOT).as_posix()) for p in files])
    firmware=ROOT/'build/source/dso138.hex'
    if not firmware.is_file():raise SystemExit('Build the source firmware first.')
    fwversion=re.search(rb'v'+re.escape(version.encode())+rb'\x00',(firmware.parent/'dso138.bin').read_bytes())
    if not fwversion:raise SystemExit('Firmware binary does not match the source version; rebuild.')
    notices=[(p,p.relative_to(ROOT).as_posix()) for p in notice_files()]
    archive(out/f'vibeDSO-v{version}-firmware.zip',notices+[(firmware,f'vibeDSO-v{version}.hex'),(ROOT/'README.md','README.md'),(ROOT/'docs/LICENSING.md','docs/LICENSING.md'),(ROOT/'docs/USB_POWER.md','docs/USB_POWER.md')])
    if args.desktop:
        desktop=args.desktop.resolve()
        if not desktop.is_file():raise SystemExit(f'Missing desktop binary: {desktop}')
        archive(out/f'vibeDSO-v{version}-Windows.zip',notices+[(desktop,desktop.name),(ROOT/'docs/PC_APP.md','docs/PC_APP.md')])
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob(f'vibeDSO-v{version}-*.zip')}
    (out/f'SHA256-v{version}.txt').write_text(''.join(f'{digest}  {name}\n' for name,digest in sorted(hashes.items())))
if __name__=='__main__':main()
