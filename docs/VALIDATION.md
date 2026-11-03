# Publication build validation (firmware / desktop 0.37)

- Python cross-build and CMake/Ninja cross-build succeed with GNU Arm GCC 15.2.1.
- Python build uses 40,144 bytes of the 48-KiB code area and 17,088 bytes static
  RAM, leaving 3,392 bytes to the top of the 20-KiB RAM (2 KiB minimum reserved).
- ELF checks pass for the initial SP, reset/SysTick/capture vector bindings,
  unresolved symbols, RAM headroom and separation from settings pages.
- 536 native checks pass across engine, buttons, settings, bootloader, LCD,
  UART/link/mirror/remote, power, clocks, generator, streaming and measurements.
  This includes 28 checks of the optional legacy clock/generator profiles, not
  the excluded OEM capture archive.
- 36 Python capture/mirror/control/calibration checks and 4 real Tk dropdown
  regression checks pass.
- 18 desktop self-test checks pass from source and again from the frozen EXE,
  including project attribution, full Apache text and dependency notices.
- Offline source/license integrity checks pass for all 11 pinned upstream files.

The settings tests include original-layout records, incomplete writes, page
transfers, repeated boot recovery and reset at each modeled transfer operation,
including partial source erase. These are digital models, not physical flash
endurance/brownout or electrical tests.

Full physical device validation is outstanding: startup, settings retention,
UART, acquisition and negative-rail regulation need a device regression check.
Vector/layout and digital model tests do not establish electrical performance
or successful execution of the complete startup path on real hardware.
