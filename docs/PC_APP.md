# vibeDSO Desktop 0.37

## Connection

Connect a3.3-V USB-UART adapter to J5, select its COM port, choose230400baud and click Connect. The app supports display mirroring, remote controls and the Debug page. Close other serial applications before connecting. Refresh rescans available ports. The application never flashes firmware automatically.

## Controls

Acquisition: timebase, trigger mode/edge/level, vertical/horizontal position, generator frequency and RUN/HOLD/Rearm. Trigger level is the calibrated ADC delta, not a voltage entry. Horizontal position is normalized from-1 to+1. Physical voltage/coupling switches cannot be overridden remotely.

Cursors/buttons: select the on-device focus, use SEL/+/−/OK/Long OK, or change time/voltage cursor mode and A/B positions. Select Cursors to draw them. With Debug selected, OK alternates power/clock and ADC diagnostics; it does not change HOLD.

Settings: explicit Save acquisition settings persists acquisition fields. Factory reset preserves zero calibration and range gains. Generator and cursor changes apply to the current session. Acknowledgements show confirmed device state; timed-out mutations are not replayed automatically.

## Display

Up to 20 changed scenes/s, with a one-second static heartbeat. Zoom changes PC display size only. Save image exports the unscaled 320x240 display to PNG. Orange arrows mean the trace is outside the viewport; red means ADC clipping and invalid voltage measurements. The mirror is reconstructed from geometry and the original font; it is not a raw RGB framebuffer stream.

All application-owned text is English. Serial driver descriptions, operating-system errors and native file dialogs may follow the operating system's language.

## Development

Run `python tools/dso138_app.py --demo` without hardware. Run `python tools/dso138_app.py --self-test analysis/app-tests.json` after native tests have created the ignored analysis directory. Build with `python tools/build_app.py --output dist` on Windows after installing requirements-build-desktop.txt. Use `python tools/package_release.py --desktop dist/vibeDSO-Desktop-v0.37.exe` to produce a Windows release ZIP with the applicable licenses and attribution. The EXE also embeds LICENSE, NOTICE and desktop dependency notices, accessible through **Licenses and attribution**.
