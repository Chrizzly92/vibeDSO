# Embedded runtime notices

The reference release is built with xPack GNU Arm Embedded GCC 15.2.1-1.1,
including newlib 4.5.0. Statically linked runtime routines retain their upstream
terms; the toolchain itself is not shipped.

- `newlib-4.5.0.txt`: supplied newlib copyright/license collection.
- `libgloss-4.5.0.txt`: supplied low-level runtime notices.
- `GPL-3.0.txt` and `GCC-runtime-exception-3.1.txt`: GCC libgcc terms, including
  the Runtime Library Exception for eligible compilations.

These are verbatim license files from that toolchain's `distro-info/licenses`.
If you build with another toolchain, check its runtime components and include
its applicable notices. Apache-2.0 does not replace runtime-library licenses.
