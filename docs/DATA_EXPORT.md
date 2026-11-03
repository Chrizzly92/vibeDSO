# UART snapshots and live export

This release uses the existing J5 USART1 interface, through a USB-UART adapter. UART runs at **230400 baud, 8 data bits, no parity, 1 stop bit, no flow control**.

## Connect

Use 3.3-V logic. Adapter TX -> DSO RX1 (reference J5 pin1), adapter RX -> DSO TX1 (reference J5 pin2), common GND (J5 pin3). Check the actual board labels. Leave the adapter's power output disconnected; retain the scope's existing supply. Run the scope normally rather than in the bootloader.

## PC receiver

For a standalone Windows application with COM selection and remote control, see [PC_APP.md](PC_APP.md). Remote command/ack details and mirror overload flags are in [REMOTE_PROTOCOL.md](REMOTE_PROTOCOL.md). The following CLI remains available.


In the project directory:

```powershell
python -m pip install pyserial
python tools/dso138_capture.py --port COM5 --output snapshot.csv
```

Display mirror (recommended for the CH340E):

```powershell
python tools/dso138_capture.py --port COM5 --mirror
```

This opens a Tk window at 2x LCD size. It mirrors the actual drawn trace spans, captions, selected menus, cursors and supply readout with the original font. No matplotlib is required. Default is maximum20 scenes/s and no disk logging. `--rate 10`, `--scale 3`, `--output view.csv` are optional. Keep the two Python tools and font binary together. The ADC and LCD rates are unchanged.

Full records, with optional plot:

```powershell
python -m pip install matplotlib
python tools/dso138_capture.py --port COM5 --full-record --stream --rate 4 --plot --output live.csv
```

Replace COM5 with the adapter's port. `python -m serial.tools.list_ports` lists ports. `--frames 20` limits a live session. Ctrl+C stops and sends STOP. If no valid capture arrives within ten seconds, check baud/wiring and acquisition mode; a long NORMAL/SINGLE capture can exceed that timeout, so use `--timeout 60` or a suitable longer value.

Offline binary test without pyserial/matplotlib:

```powershell
python tools/dso138_capture.py --decode-file analysis/link-fixture.bin --output decoded.csv
```

CSV includes sequence, publication timestamp, packet kind/flags, skipped publications, record-relative time, ADC min/max, calibrated voltages, exact configured interval/core clock, source record length, zero ADC, sensitivity/coupling codes and gain. Publication milliseconds are a boot-time counter, not an ADC trigger timestamp. `t_record_s` starts at zero for each record. Do not concatenate records as if they were gapless.

## Compact display scenes

`--mirror` and default `--stream` request MIRROR20. Changes are limited to20 packets/s; an unchanged view sends only a1-second heartbeat. Closing the Tk window or Ctrl+C sends STOP. The viewer redraws only the latest complete scene in a receive batch. Header/footer include the device's actual cached captions; measurement text retains its existing4-Hz update limit.

A scene has300 pairs of byte-sized drawn y coordinates plus UI state and text descriptors, at most854 bytes total (about37.1 ms at2304008N1). At20/s that is at most17080 bytes/s, within the23040-byte/s UART budget. This is a configured cap, not a hardware benchmark. Full-screen RGB565 bitmaps would need153600 bytes per image. DMA TX buffer remains2100 bytes; no framebuffer is allocated on the MCU.

Display CSV explicitly labels pixel top/bottom and approximate visible voltages. These are clipped, interpolated screen geometry, not raw ADC samples. Use SNAP/full-record for quantitative ADC export. Scene time is relative to the left screen column; its header interval is Q16 core-clock ticks per pixel. UI revision counts include caption/marker/power changes, so skipped revisions are not lost ADC records. Publication time is when the scene was read, not an acquisition timestamp.

## What full-record live means

SNAP exports the latest available completed snapshot, including HOLD. STREAM selects new completed snapshots at a maximum of 1..5 packets/s (default4). A raw 1024-sample packet is 2100 bytes, taking about91 ms on a 230400-baud 8N1 link. Transfer is via DMA while acquisition continues. Actual cadence depends on capture time and display/processing load. There is no measured device-throughput guarantee yet.

UART carries at most 23040 bytes/s at this baud before framing overhead. A continuous 1-MS/s stream stored as 16-bit ADC codes would need 2000000 bytes/s, so it is not offered. Published sequences can be skipped deliberately by the cadence limit, or because a packet is still in flight. The header reports missing source-publication sequences. CRC and sequence numbers also let the receiver detect transmission loss. Stopping does not truncate a packet already being transmitted.

Normal records export all published raw ADC samples, independent of cursor pixels. Peak-roll records export minimum/maximum pairs. If the source has more than512 bins, adjacent pairs are aggregated into up to512 envelope bins, preserving all extrema and doubling the declared interval. This is marked in the packet. The original count is retained, including odd final bins. Raw 100-kS/s roll samples are not streamed continuously or reconstructed from envelopes.

## ASCII commands

Commands are uppercase, newline-terminated. CR is ignored. Receive buffer/parser length is bounded.

- `VIEW\n`: send one complete display scene.
- `MIRROR\n` or `MIRROR 20\n`: display scenes, maximum20/s.
- `MIRROR 1\n` .. `MIRROR 20\n`: display maximum cadence.
- `SNAP\n`: send the latest available record once; may wait until a record exists.
- `STREAM\n`: live mode, maximum4 packets/s.
- `STREAM 1\n` .. `STREAM 5\n`: set maximum packet cadence.
- `STOP\n`: stop requesting new packets.

There are no textual ACK lines. A binary data packet is the response. Boot text before LinkInit is ignored by the receiver's magic/CRC parser. The PC script uses pyserial only for the port; its decoder and CSV output use the Python standard library. Matplotlib is optional. Physical COM and Tk/matplotlib GUI operation have not been tested here. Native C++ incremental LCD rasters and the Python mirror are compared pixel-for-pixel in eleven UI states.

## Wire format v1

Little-endian. Header48 bytes, payload, then CRC32/IEEE (same as zlib.crc32) over header+payload.

| Offset | Field |
|---:|---|
| 0 | ASCII DSO1 |
| 4 | u8 protocol version=1 |
| 5 | u8 kind: 1 raw, 2 min/max, 3 display scene |
| 6 | u16 header size=48 |
| 8 | u32 source-publication sequence |
| 12 | u32 publication milliseconds |
| 16 | u64 interval in core-clock ticks |
| 24 | u32 nominal core frequency |
| 28 | u16 output sample/bin count |
| 30 | u16 flags |
| 32 | u16 calibrated zero ADC |
| 34 | u8 sensitivity code4..12 |
| 35 | u8 coupling: DC0/AC1/GND2 |
| 36 | u16 range gain Q10 |
| 38 | u16 original source count |
| 40 | u32 skipped source publications since prior queued packet |
| 44 | u16 payload byte count |
| 46 | u16 reserved=0 |

Flags: bit0 trigger indication at publication; bit1 HOLD; bit2 peak-roll; bit3 record stream can have gaps; bit4 adjacent bins aggregated; bit5 ADC clipping; bit6 current acquisition overrun; bit7 last peak bin is partial. Maximum packet2100 bytes. Payload kind1: u16 ADC codes; kind2: repeated u16 min,u16 max. ADC values are12-bit in16-bit storage.

Sensitivity code4..12 maps to 5,2,1,.5,.2,.1,.05,.02,.01 volts/div. Voltage conversion matches the firmware:

`V = (adc-zero) * V_per_div * 275 * gain_q10 / (1092 * 1024 * 25)`

The included range gains and remaining calibration uncertainty apply equally to CSV. Clock values are configured nominal values; oscillator accuracy has not been measured.

## Why not native USB yet?

The reference schematic connects J4 D-/D+ through R29/R30 to PA11/PA12; actual resistor/connector population on the user's board has not been inspected. Hardware reference: [JYE manual/schematic](https://jyetech.com/wp-content/uploads/2018/07/dso138-user-manual-13803k-13804k.pdf).

The current single-ADC source profile uses PLL/core56 MHz and ADC14 MHz for nominal1 MS/s. STM32F103 USB requires48 MHz from the PLL via /1 or /1.5. Neither56 nor56/1.5 meets that. A USB-compatible48/72-MHz profile gives at most12-MHz rated ADC clock with the available prescalers in this design, about857 kS/s for continuous14-cycle conversions. Keeping1 MS/s with USB would need a larger acquisition/ADC redesign rather than silently changing the sampling rate or overclocking the ADC. No native CDC stack/profile is included. Source: [STM32F103x8/xB datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf).

## Kind3 display payload, schemas1/2/3/4

Header count=300; interval field is Q16 core ticks per column (divide by core_hz*65536 for seconds). Sequence is UI revision, original count is the displayed record count. Raw flags only provide context; geometry is authoritative.

| Payload offset | Field |
|---:|---|
| 0 | schema=4 (receiver also accepts schemas1/2/3) |
| 1..7 | focus, edge, GEN, M selected, C selected, state, HP selected |
| 8,10 | i16 zero/trigger y |
| 12,14 | i16 HP bar left/right |
| 16,17 | cursor flags (visible/voltage), selected A/B |
| 18,20,22,24 | u16 cursor x0/x1/y0/y1 |
| 26,27 | text count, reserved |
| 28,30 | u16 reference/power colors (native BGR565) |
| 32..38 | seven power ASCII characters |
| 39 | schema2/3/4 clip flags; see REMOTE_PROTOCOL.md |
| 40..639 | 300 repeated u8 top,bottom, inclusive y17..216 |
| 640..end | text descriptors and ASCII bytes |

Edge bit0=rising, bit1=selected; GEN bit0=enabled, bit1=selected. State0=AUTO,1=T'D,2=HOLD,3=OVR,4=WAIT. A text descriptor is u16 x,u8 y,u8 pixel_width,u16 color,u8 selected,u8 length, followed by length characters (maximum10). At most9 texts, maximum802 payload bytes. Layout/icon/font schema is versioned independently at payload byte0. The PC rasterizer applies the same one-pixel physical LCD translation and clipping as Screen.cpp. Existing protocol version1 kind1/kind2 is unchanged.
