# Provenance and third-party notices

The distributable attribution notice is [NOTICE](NOTICE). License scopes and
redistribution requirements are described in [LICENSE.md](LICENSE.md).

The board support, original font and API/UI portions in `113-13801.cpp`, `Board.*`,
`Command.*`, `Common.*`, `Screen.*` and `libdso138.h` derive from JYE Tech's
113-13801-037 source package. They were ported to C++17 and modified for display
addressing, acquisition/control integration and startup behavior. Original JYE
notices and [permission](vendor/JYE-LICENSE.txt) remain. `tools/dso138_font.bin`
is the original LCD font used by the PC renderer. See also its
[font notice](docs/pc-licenses/JYE-font.txt).

The capture engine, feedback controller, protocol, desktop frontend, settings
store and supporting tests/tools are new implementations. This project is not a
full decompilation of OEM061. The OEM HEX/ELF and vendor capture library are not
included and are not needed to build the default source engine.

Device definitions and system-clock support use official ST and Arm components
listed in [the upstream manifest](vendor/upstream-manifest.json). Their original
notices and licenses are retained. See [licensing and distribution](docs/LICENSING.md).

Calibration reference data comes from one device and is not universal. Screenshots
are project-created images; the DSO captures/renderings contain the JYE LCD font.
The Windows screenshots show the project's own Tk application. Provenance for each
image is in [docs/images/README.md](docs/images/README.md).

Frozen desktop dependencies have their notices in [docs/pc-licenses](docs/pc-licenses).
Embedded runtime notices are in [docs/firmware-licenses](docs/firmware-licenses).
