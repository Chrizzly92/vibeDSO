# Remote control

UART2304008N1. Existing SNAP/VIEW/STREAM/MIRROR/STOP commands remain. Control requests use `@<id> <command>\n`, id1..65535, uppercase ASCII. Maximum input line39 bytes including prefix, before newline; CR ignored. Overlong lines discarded through newline. Host keeps one request in flight, waits for matching kind4 acknowledgement and does not automatically retry timed-out mutations. Firmware ignores an additional request while an acknowledgement is pending. There is no duplicate-ID replay cache. RX runs in the main loop, never executes flash/control work in an ISR. Control TX shares the frozen2100-byte DMA buffer; it cannot overwrite an in-flight scene. Replies take priority over the next scene and do not change scene cadence/publication tracking.

| Command | Value / action |
|---|---|
| GET | Read actual state |
| TB n |1..24, same enumeration as libdso138.h;500s/div..10us/div |
| MODE n |0 AUTO,1 NORMAL,2 SINGLE |
| EDGE n |0 falling,1 rising |
| TL n |Signed ADC delta relative to calibrated zero; effective absolute threshold must be0..4095 |
| VP n |Vertical translation -255..255 pixels |
| HP n |Normalized horizontal -100..100 represents -1..1 |
| HOLD n |0 run,1 hold |
| ARM |Clear HOLD and explicitly restart/rearm current mode |
| GEN n |1..100000 Hz, preserve enable state |
| GENON n |0 off,1 on, preserve frequency |
| FOCUS n |0 timebase,1 mode,2 edge,3 trigger,4 HP,5 VP,6 GEN,7 M,8 C,9 Debug |
| KEY n |0 SEL,1 plus,2 minus,3 OK,4 long OK |
| CMODE n |0 time cursors,1 voltage cursors |
| CA n / CB n |Cursor A/B coordinate, x10..309 or y17..216 |
| SAVE |Persist existing acquisition settings, unchanged writes skipped |
| RESET SETTINGS |Factory defaults, calibration/selectors retained |

Invalid values are rejected, not silently clamped. HOLD survives ordinary timebase/mode changes; ARM/HOLD0/RESET explicitly resume. Mechanical range/coupling have no remote setters. Direct setters are volatile until SAVE; virtual KEY actions retain local key semantics including existing flash writes/calibration actions.

## Kind4 acknowledgement

Same DSO1/v1 little-endian48-byte header and CRC32/IEEE as records. Kind4, count0, payload32, total84 bytes. Sequence=request ID, publication_ms=response creation time; interval0. Scene schemas and raw kinds remain independent. Payload:

| Offset | Type / value |
|---:|---|
|0 |u8 schema1 |
|1 |u8 result:0 OK,1 unknown command,2 invalid argument,3 reserved busy,4 flash failure |
|2 |u16 request ID |
|4..9 |u8 timebase,mode,edge,focus,sensitivity,coupling |
|10 |u16 flags: bit0 HOLD,bit1 GEN enabled,bit2 measurement panel,bit3 voltage cursors,bit4 selected cursor B |
|12 |i16 VPOS |
|14 |u16 HP in original sample-position units |
|16 |i16 trigger delta ADC |
|18 |u16 record length |
|20 |u32 generator target Hz |
|24 |i16 calibrated zero offset ADC |
|26,28 |u16 cursor A/B coordinate in active mode |
|30 |u16 reserved0 |

The state follows actual command execution even on errors. Host state polling is1 Hz. Bytes must be decoded by kind; a control reply is not a measurement record. The CLI receiver filters kind4 from CSV output.

## Display mirror schema

The firmware emits kind3 packets with payload schema4. Packet layout is documented
in [DATA_EXPORT.md](DATA_EXPORT.md); schema numbering is independent of the
firmware release version.

Payload byte39 contains clipping flags: bit0 top viewport overflow, bit1 bottom
viewport overflow, bit2 upper ADC saturation, bit3 lower ADC saturation. ADC
saturation also sets its corresponding viewport warning. Orange triangles indicate
viewport clipping; red triangles indicate ADC saturation. Triangles have seven
rows, logical center x301, at y17..23 / y210..216, maximum width13. The renderer
applies the one-pixel physical LCD translation.

Power text/color are diagnostic telemetry. The normal header at logical
x256..311 shows D (focus9); a red PWR! warning takes its place on a power fault.
HP footer text fields use x0/120/240, each width80. The desktop receiver accepts
schemas1..4 and selects the corresponding geometry. Schema1 has no clip flags;
schema2 uses three-row triangles centered at x305; schema3 uses the seven-row
triangles and draws power telemetry in the header.
