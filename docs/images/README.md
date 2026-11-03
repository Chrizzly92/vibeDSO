# Image provenance

Captured on 2026-11-03 for the README.

- `desktop-live.jpg`: actual vibeDSO Desktop 0.37 window connected to the user's CH340/DSO138; internal generator 1 kHz, DC coupling, 1 V/div, 200 µs/div.
- `desktop-cursors-live.jpg`, `desktop-debug-live.jpg`: actual application screenshots of the same live connection, with Cursor and Debug focus respectively.
- `dso-live.png`, `dso-cursors-live.png`, `dso-debug-live.png`: LCD-region crops of those three screenshots, rectangle (left 50, top 275, width 960, height 720). The capture contains the app's scaled 320×240 scene. These are not device photographs or lossless raw scene exports; the source screenshots are JPEG. No waveform or numeric content was retouched.
- `clipping-simulation.png`: rendered from the native C++ mirror-test fixture `analysis/mirror-9.bin`, using the project's Python rasterizer. Synthetic ADC extrema exercise red saturation markers. This is an LCD/register-model example, not a hardware measurement or full analog simulation. Regenerate the fixture with `python tools/test_host.py --mirror`.
- `splash.png`: firmware LCD-model startup render, generated from the included source.

The live captures illustrate features, not certified voltage/frequency accuracy.
The cursor footer frequency is reciprocal cursor separation, not the automatic
frequency measurement. Screenshots are illustrative; see validation notes for
the release's hardware-test coverage.
