# Licensing and distribution

## Project license

vibeDSO work is licensed under Apache-2.0. [NOTICE](../NOTICE) identifies
Chrizzly92 and https://github.com/Chrizzly92/vibeDSO as the original source.
The unmodified license text is in [LICENSE](../LICENSE); component-specific
terms and attribution requirements are described in [LICENSE.md](../LICENSE.md).

## Components

- Board/UI code, API declarations and the LCD font include JYE Tech material;
  see [JYE permission](../vendor/JYE-LICENSE.txt).
- Official ST `cmsis-device-f1` device definitions and system-clock support are
  included unchanged at commit `c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22`,
  under [Apache-2.0](../vendor/cmsis-device-f1/License.md).
- The GNU/Cortex-M3 subset of Arm CMSIS-Core 5.9.0 is included at commit
  `2b7495b8535bdcb306dac29b9ded4cfb679d7e5c`, under
  [Apache-2.0](../vendor/cmsis-core/LICENSE.txt).
- Settings storage, flash operations, startup and interrupt handling are
  project implementations. Peripheral control uses register/CMSIS access.
- Firmware runtime and desktop dependency terms are supplied in
  [firmware-licenses](firmware-licenses) and [pc-licenses](pc-licenses).

[upstream-manifest.json](../vendor/upstream-manifest.json) records imported
files' commit URLs and SHA-256 hashes. Verify them offline with
`python tools/check_licenses.py`.

## Release packages

Run the documented build and validation steps, then:

```sh
python tools/package_release.py
```

Add `--desktop PATH_TO_EXE` to package the Windows application as well.
The generated source, firmware and Windows ZIPs carry LICENSE, NOTICE,
the license scope/provenance guide and applicable third-party terms.
Distribute binaries with these notices. Source archives omit Git metadata,
build products and local toolchains.
