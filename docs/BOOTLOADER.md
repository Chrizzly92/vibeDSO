# Software bootloader entry

## Use

1. Install vibeDSO using the hardware flashing method described in the README.
2. Disconnect the Desktop app from COM; no serial viewer may hold the port.
3. Hold PLUS and MINUS, then power on or press hardware reset. Do not hold SEL/OK.
4. Release when Firmware update / UART1 / CH340 ready appears. The display is latched; it is not a live confirmation of host connection.
5. Connect STM32CubeProgrammer or another STM32 ROM UART programmer via the existing3.3-V CH340 TX/RX/GND on USART1/J5. Choose UART and the correct COM port;115200 baud,8E1 (even parity). The programmer performs the binary0x7F synchronization. The normal2304008N1 mirror protocol is not running here.
6. Program the new HEX at its embedded0x08000000 addresses. To retain settings/calibration, erase only application pages covered by the image; do not mass erase the MCU. Settings occupy0x0800c000..0x0800c7ff. A full-chip erase loses these saved values.
7. Power-cycle/reset without holding buttons after programming. No jumper restoration is needed. If the programmer uses GO, reset is still the supported way to resume normal device operation.

Normal startup waits500ms on the early splash. PLUS+MINUS must be sampled together for40 consecutive1-ms ticks; any other key or release restarts this count. After the window, normal operation continues. Runtime PLUS+MINUS still performs the existing two-second factory-settings reset, so use the update chord from power-on.

## Implementation and limits

Only the source-engine firmware has this update gate. It precedes factory test selection, EEPROM unlock/init, USART, SysTick IRQ, timer and ADC initialization. PA7 generator and PB9 inverter are explicitly held low during startup. No settings or application flash is written by entering the mode.

ROM base0x1ffff000 for STM32F103C8. MSP must be4-byte aligned inside20-KiB SRAM; the reset handler must be a Thumb address inside the2-KiB system ROM. Invalid vectors show Bootloader error / ROM vectors invalid with raw SP/PC values and require power-cycle, rather than falling through to runtime factory reset.

Handoff disables SysTick and the two implemented NVIC banks, clears pending SysTick/PendSV, resets used peripherals, disables DMA, switches to HSI with bounded readiness checks, selects ROM VTOR, clears processor masking/control state and loads ROM MSP in a naked assembly tail jump. No C++ stack access follows MSP replacement. GPIO keeps generator/inverter low and LCD controls inactive. Settings and BOOT option/pins are unchanged.

This is the existing ST ROM loader, not USB DFU and not a custom flash loader. Jumper/SWD recovery remains necessary if the application itself cannot start. No PC update button or automatic firmware programming has been added.

Verified:15 native gate/vector/register checks, ARM ELF/vector layout, actual linked startup ordering, and no stack operations after the MSP change. Real ROM response, successful UART programming and normal restart require physical hardware verification; none was performed for this release.

References: [ST AN2606](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf), [ST software jump guidance](https://community.st.com/stm32-mcus-60/how-to-jump-to-system-bootloader-from-application-code-on-stm32-microcontrollers-71).
