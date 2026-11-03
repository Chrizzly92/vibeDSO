# USB power: board-specific modification notes

The author reports operating a modified DSO138 from USB 5 V while retaining U4 (79L05). These notes clarify the described wiring; they are not a verified modification guide for every DSO138 or clone revision.

## What the firmware controls

`src/power_config.hpp` sets the target to an 8 V magnitude on the raw negative rail **before U4**. PA6 senses that node through R41/R42. The on-board switching inverter is driven by bounded PWM. U4 regulates the raw negative rail to the analog−5V supply; its output is not independently measured by this firmware. Positive 5 V and 3.3 V supplies are also not measured.

## Reported modification

- USB VBUS feeds the board's +5 V rail.
- U5, the original positive 5 V regulator, is removed; its former input and output nets are connected so the original upstream supply/inverter feed also receives 5 V. Do not identify these pads solely as the outer pins: check the exact board and regulator pinout and do not bridge the ground pad.
- U4 remains installed. No additional external boost module was used on this development setup; that does not establish adequate inverter headroom on every board or under every load.
- Disconnect or physically remove the original higher-voltage barrel-input path so an8–12V adapter cannot reach USB VBUS or the +5 V rail. Treat leaving that path usable as an electrical conflict, not just a cosmetic choice. Verify D2's actual function and connections on your board.

Before attaching USB or a PC, check the resulting connections with power off. Test with one appropriate supply at a time and measure the +5 V, 3.3 V, raw negative and regulated−5V rails externally. The firmware display cannot verify all of these rails. Low-generator-frequency/load transients have occurred during development, so do not interpret steady-state regulation as exhaustive power-stage validation.

## Reference

JYE's original schematic lists U5 as 78L05 and U4 as 79L05. Use the schematic matching your board, not a DSO138mini schematic or an assumed TO-92 orientation: [JYE DSO138 manual and schematic](https://jyetech.com/wp-content/uploads/2018/07/dso138-user-manual-13803k-13804k.pdf).
