# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**AOA DF 2m** — A Windows desktop application for Angle-of-Arrival (AOA) radio direction finding on the 2-meter amateur radio band (~144–148 MHz). It interfaces with a custom SDR hardware device over serial port, performs real-time IQ DSP, demodulates FM audio, and displays a bearing estimate and RF spectrum.

## Building

Open `wx1.vcxproj` in **Visual Studio 2019** (v142 toolset). Build the **x64** configuration (Debug or Release). There is no command-line build script.

**Required dependencies (must be installed separately):**
- **wxWidgets 3.2.6** — expected at `C:\Users\jcroo\wxWidgets-3.2.6\` (x64 builds) or `D:\wxWidgets-3.2.6\` (Win32 builds)
- **Intel oneAPI IPP** — expected at `C:\Program Files (x86)\Intel\oneAPI\ipp\latest\include`
- **PortAudio** — `portaudio_x64.lib` and `portaudio_x64.dll` are included in the repo root

There are no automated tests.

## Runtime Configuration

**`comport.txt`** (in the working directory / Release folder): contains the COM port number on the first line (e.g., `3` for COM3). The app reads this at connect time.

## Architecture

### Threading Model
- **Main thread** — wxWidgets event loop; a 125ms `wxTimer` triggers UI refresh
- **Data thread** (`CRadio::DataThread` / `RXDataLoop`) — reads raw ADC frames from the serial port and runs the DSP pipeline; spawned on Connect
- **PortAudio callback** (`patestCallback`) — interrupt-level audio output; reads from `audioOutBuf` ring buffer filled by the data thread

Synchronization is implicit via ring-buffer read/write pointers (`audioOutWrPtr`/`audioOutRdPtr`); there are no explicit mutexes.

### DSP Pipeline (in `CRadio::ProcessRawToIQ` → `DoRXDSP`)

1. **Serial ingestion** — 256-byte packets arrive at 115200 baud; each packet contains 64 interleaved CH1/CH2 ADC samples (offset-binary, ~12-bit effective)
2. **DC removal** — 4th-order Chebyshev Type-I high-pass IIR at ~4 kHz / 96 kSPS using `IppsIIRState_64f` (64-bit state required for numerical stability near z=1)
3. **Quadrature mixing** — `ippsTone_32fc` generates a complex LO; CH1 and CH2 are mixed with a quarter-period phase offset to account for staggered ADC sampling
4. **FIR lowpass / decimation** — 2048-tap Bartlett-windowed FIR (normalized cutoff 0.025, applied at 96 kSPS) applied to both channels; then 2:1 decimation to 48 kSPS
5. **AOA estimate** — phase difference between CH1 and CH2 computed via conjugate product accumulation (`Ch2 * conj(Ch1)`), then `atan2f` → degrees; result stored in `myStatus->phaseDelta`
6. **FFT spectrum** — 512-point FFT with Hann window on summed IQ; magnitude → dBFS; result fftshifted into `myStatus->RFFreqPlot[256]`
7. **FM demod + audio** — `ippsPhase_32fc` on summed IQ, differentiated sample-to-sample → instantaneous frequency; 2048-tap Hamming bandpass FIR (200 Hz – 3 kHz @ 48 kSPS) → `audioOutBuf`

### Frequency Tuning (`CRadio::SetFreq`)
The hardware uses a two-LO scheme: fixed first LO at 159.6 MHz, second LO at `(freq - 159.6 - 0.024) MHz` synthesized as `LO1 / (Idiv + Fdiv/64)`. Tuning command is `'f'` + 3 encoded bytes sent over serial.

### Key Data Structures
- `RadioStatus` — shared state between DSP thread and UI: `phaseDelta` (bearing degrees), `RFFreqPlot[256]` (spectrum dBFS), `RXFreq`
- `CRadio` — owns all IPP buffers, PortAudio stream, serial handle, and threads; allocated in `MyFrame` constructor

### UI Layer (`wx1.cpp` / `frame1.h`)
- `BasicDrawPane` — custom `wxPanel` that renders all plots via `wxDC` in its `render()` method; flagged dirty by `RFModified`
- `Plot()` / `Plot2()` — generic oscilloscope-style plot widgets drawn directly with `wxDC`
- `DrawBearing()` — horizontal bar indicator showing `phaseDelta` from −90° to +90°

### Files
| File | Purpose |
|------|---------|
| `wx1.cpp` | App entry point, UI rendering functions, `MyFrame` / `BasicDrawPane` implementation |
| `CRadio.h` / `CRadio.cpp` | All radio hardware I/O, DSP, and audio logic |
| `frame1.h` | wxFormBuilder-generated class declarations for `MyFrame` and `BasicDrawPane` |
| `DSP.cpp` | Polyphase resample kernel coefficients (512-element sinc, unused in main pipeline currently) |
| `portaudio.h` / `portaudio_x64.lib/.dll` | PortAudio library, bundled in repo |
