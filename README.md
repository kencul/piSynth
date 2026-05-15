# PiSynth

[![Tests](https://github.com/kencul/piSynth/actions/workflows/tests.yml/badge.svg)](https://github.com/kencul/piSynth/actions/workflows/tests.yml)

**PiSynth** is a polyphonic plucked-string synthesizer written in C++20 for the Raspberry Pi. It is a working instrument and a documented engineering project, where each chapter of the repository is a distinct technical milestone, starting from a raw ALSA sine wave and building up to a full synthesizer with a real-time browser interface.

---

## Technical Highlights

* **Real-time Audio Engine**: ALSA PCM with a dedicated `SCHED_FIFO` priority-80 audio thread for deterministic, low-latency output.
* **Physical Modeling**: Extended Karplus-Strong plucked string synthesis. Triangle-wave initialization with a configurable pluck position shapes the attack character; a waveguide-inspired pickup position applies a harmonic comb filter to the output. Tuning-compensated feedback gain and a DC blocker complete the model.
* **Thread Safety on ARM**: C++ atomics with explicit `acquire`/`release` ordering across the MIDI, audio, and web threads, accounting for ARM's weak memory model.
* **Advanced DSP**:
  * Zero-Delay Feedback State Variable Filter (SVF) with MIDI CC keytracking
  * Master bus: Stereo Chorus, Freeverb reverb, and Ping-Pong Delay
  * Sample-accurate parameter smoothing to eliminate zipper noise
  * TPDF dithering for 16-bit output paths; float passthrough for 24/32-bit devices
* **Full-Stack Control**: Browser UI with real-time FFT spectrum analyzer, live waveguide state display, RMS/peak meter, preset management (stored in `~/.local/share/pi-synth/presets/`), and multi-client WebSocket sync.
* **Plug-and-Play Hardware**: Automatic USB audio device discovery with runtime sample-rate and bit-depth negotiation. MIDI controllers auto-connect on startup and hotswap without restarting.

---

## Development Journey

The repository is organized as a series of chapters. Each is a self-contained, buildable program with its own documentation covering the engineering decisions at that stage.

| # | Title | Key Focus |
| :---: | :--- | :--- |
| [01](./chapters/01_basic_sine) | Basic Sine | **ALSA PCM**: buffer management, hardware params, xrun recovery |
| [02](./chapters/02_midi_monitor) | MIDI Monitor | **ALSA sequencer**: async MIDI event handling |
| [03](./chapters/03_simple_midi_synth) | Simple Synth | **MIDI to audio**: bridging input to real-time output |
| [04](./chapters/04_poly_midi_synth) | Poly MIDI Synth | **Thread safety**: C++ atomics and ARM weak memory model |
| [05](./chapters/05_pluck_synth) | Pluck Synth | **Physical modeling**: Karplus-Strong waveguide synthesis |
| [06](./chapters/06_pluck_synth_cc) | CC Control | **MIDI CC**: parameter mapping with log/exp scaling |
| [07](./chapters/07_pluck_synth_effects) | Effects Bus | **DSP**: Zero-Delay SVF, Freeverb reverb, Chorus, parameter smoothing |
| [08](./chapters/08_web_ui) | Web UI | **Web integration**: uWebSockets, live FFT spectrum, waveguide visualization |
| [09](./chapters/09_plug_and_play) | Plug and Play (Final) | **Systems integration**: auto device discovery, hotswap, presets |

---

## Setup & Build

### Hardware

* **Raspberry Pi 5 4GB** running Pi OS Lite
* Any class-compliant **USB Audio Interface** (Apple USB-C dongle, Focusrite Scarlett, etc.)
* Any class-compliant **USB MIDI controller**

### 1. Flash Pi OS

Download [Raspberry Pi Imager](https://www.raspberrypi.com/software/) and flash **Raspberry Pi OS Lite** to an SD card. During setup, configure your hostname, username, and an SSH public key so you can connect headlessly.

### 2. SSH into the Pi

```bash
ssh <user>@<hostname>.local
```

### 3. Update the system

```bash
sudo apt update && sudo apt full-upgrade -y
```

### 4. Install build dependencies

```bash
sudo apt install build-essential cmake ninja-build libasound2-dev
```

> uWebSockets and PFFFT are bundled under `deps/`, so no additional installs needed.

### 5. Clone and build

```bash
git clone --recurse-submodules https://github.com/kencul/piSynth
cd synth
mkdir build && cd build
cmake ..
ninja
```

All executables land in `build/bin/`.

> **Note:** `web/index.html` is embedded into the binary at CMake configure time, not at `ninja` build time. If you edit `index.html`, re-run `cmake ..` before `ninja` to pick up the changes.

### 6. Run

```bash
./bin/synth
```

The program will auto-connect any plugged-in MIDI controllers and scan for a USB audio output. Open `http://<hostname>.local:9002` in a browser on the same network to access the control dashboard.

## Tests

The DSP core is covered by a [Catch2](https://github.com/catchorg/Catch2) test suite in [`tests/`](./tests/). Each test validates a quantitative claim about the audio engine — not just that code runs, but that it produces the correct signal.

| File | What is verified |
| :--- | :--- |
| `test_pluck.cpp` | Pitch accuracy (±2 cents, via autocorrelation with parabolic interpolation); decay rate (±10% of requested dB/sec, measured via Goertzel at the fundamental bin to eliminate harmonic bias); DC blocker convergence; pickup position comb filter (null below −40 dB, measured −43 dB; boost at +1.9 dB vs fundamental) |
| `test_svf.cpp` | −6 dB at cutoff for Q = 0.5 (±0.3 dB); −3 dB point at cutoff with Butterworth Q = 1/√2 (±0.3 dB); both verified from 200 Hz to 18 kHz — residual deviation is a known property of the ZDF trapezoidal integrator, not a bug; passband flatness; −40 dB/decade stopband rolloff; resonance peak |
| `test_adsr.cpp` | Stage transitions; attack and release timing (±2 samples); sustain hold; kill ramp within `KILL_MS`; reset |
| `test_ring_buffer.cpp` | FIFO ordering; full/empty conditions; SPSC correctness under concurrent access (validates `acquire`/`release` ordering on ARM) |
| `test_smoothed_value.cpp` | Immediate snap on `reset()`; 63% convergence after one time constant; asymptotic tail termination via snap threshold; re-targeting |
| `test_lfo.cpp` | Output bounds [−1, 1] for all five shapes; shape values at known phase landmarks (e.g. sine quarter-cycle, triangle peak/trough); cycle period within ±1 sample across rates 1–10 Hz — enabled by a double-precision phase accumulator that eliminates float drift over long cycles |

To build and run:

```bash
cd build
ninja synth_tests
ctest --output-on-failure
```

---

## Credits
- [uWebSockets](https://github.com/uNetworking/uWebSockets) by Alex Hultman (Apache 2.0)
- [PFFFT](https://github.com/marton78/pffft) by Julien Pommier, fork maintained by marton78 (BSD-like)

This project is licensed under the [MIT License](LICENSE).

---

Built by [Ken Kobayashi](https://www.kenmusic.net/)
