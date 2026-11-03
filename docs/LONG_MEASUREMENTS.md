# Long roll measurements and cursors

## Long statistics

Peak-roll acquires at100 kS/s. Numeric PP/RMS/extrema/mean and timing use raw samples fed into bounded integer accumulators. Each statistics window lasts at least1 s, otherwise the nominal visible12-division timespan, up to6000 s at500 s/div. The last completed window and the current partial window are combined. After startup the statistics therefore span between one and two such windows. Before the first complete window they use the available partial data. This is not an exact sliding copy of the visible trace.

Count/sum/sum-of-squares/extrema require constant memory; the waveform samples for these long windows are not stored. RMS is relative to calibrated zero and includes DC. Clipping propagates into numeric validity.

A separate Schmitt edge tracker retains up to four complete rising-to-rising cycles and their positive pulse widths. It can measure periods spanning many short DMA buffers, including0.1 Hz in tests. Initial level discovery and multiple cycles take time. A moving threshold or changed frequency can temporarily suppress timing until periods become consistent. At least8 raw samples per period, sufficient swing and stable periods remain required; roll timing is therefore limited to about12.5 kHz at100 kS/s. If no new complete cycle arrives, old timing expires after the greater of two seconds or three measured periods. Changing range/coupling restarts the active capture so data from different analog settings are not merged.

The statistics/history reset on StartCapture. AUTO roll can accumulate continuously; NORMAL/SINGLE still form a bounded triggered record. A record shorter than a complete slow period cannot supply a valid full-period measurement. HOLD retains the stopped record; no elapsed-wall-time decay is applied while sample acquisition is stopped.

Trigger centering uses the extrema midpoint. A25-percent-duty rectangular signal therefore gets the same central threshold as a50-percent-duty one. VPOS zero calibration still uses the DC average on GND.

## Cursor operation

SEL reaches C after M. Two cursors appear only while C is selected.

- Plus/minus move the selected cursor, including existing decade acceleration.
- Short OK selects A/B, without toggling HOLD.
- Long OK changes TIME/VOLT, once per hold.
- TIME: vertical lines; footer A/B time relative to the fixed central reference, dT magnitude and F=1/dT. Equal positions display F--.
- VOLT: horizontal lines; footer A/B voltages relative to the zero-position reference and signed dV=B-A.
- Active line orange, other line blue. The active A/B footer field is framed.

Cursors measure the displayed axes. TIME positions can lie between acquired samples; interpolation/one-pixel movement does not add ADC resolution. Absolute A/B time is relative to the fixed reference, not an independently measured trigger timestamp. Voltage cursor positions use range calibration and VPOS. Acquisition samples, automatic measurements and UART raw payloads are unaffected by cursor drawing.

Only old/new cursor columns or row spans are redrawn. Leaving C restores the previous plot without stale lines. The permanent HP reference remains independent.

HP displays two decimal places, -1.00..0.00..+1.00, with sample-based positioning and a fixed reference/moving range bar.
