# License scope and attribution

Copyright 2026 Chrizzly92. Original project: https://github.com/Chrizzly92/vibeDSO

New vibeDSO code, documentation, tests and tools, and our modifications to the
inherited JYE files, are licensed under the **Apache License 2.0** in [LICENSE](LICENSE).
This permits private and commercial use, modification, forks and sale, including
closed-source derivatives, subject to the license conditions. No obligation to
publish modified source is added.

When redistributing relevant vibeDSO code or binaries, include the license and
retain the applicable attribution from [NOTICE](NOTICE), including the original
project URL. Apache-2.0 section 4 permits that notice in an accompanying NOTICE
file, documentation or the usual third-party-notices display. It does not require
a prominent advertising link, a clickable UI link or an unchanged boot screen.
Mark files you modify and retain applicable original source notices. Do not imply
that the original author or third parties endorse your derivative.

## Third-party scope

| Component | Applicable terms |
| --- | --- |
| Original portions of `src/{113-13801.cpp,Board.*,Command.*,Common.*,Screen.*,libdso138.h}` and `tools/dso138_font.bin` | [JYE permission](vendor/JYE-LICENSE.txt); retained copyright; our changes are additionally Apache-2.0 |
| `vendor/cmsis-device-f1` | Official ST component [Apache-2.0](vendor/cmsis-device-f1/License.md), pinned commit in [manifest](vendor/upstream-manifest.json) |
| `vendor/cmsis-core` | Arm CMSIS-Core 5.9.0, [Apache-2.0](vendor/cmsis-core/LICENSE.txt), pinned commit in the manifest |
| Firmware runtime libraries supplied by the toolchain | [newlib/libgloss notices and GCC runtime exception](docs/firmware-licenses/README.md) |
| Bundled desktop dependencies | [Python, Tcl/Tk, pyserial and PyInstaller terms](docs/pc-licenses) |

Third-party terms continue to apply to their original material. The project
license cannot grant rights to unrelated OEM binaries, hardware designs, marks,
or components that are not part of this source release.

The settings store, flash operations, startup assembly and interrupt handlers
are project implementations. Device definitions and system-clock support use the
ST and Arm components listed above. See [licensing and distribution](docs/LICENSING.md)
for component provenance and release packaging.
