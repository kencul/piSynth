# PiSynth

**PiSynth** is a high-performance, polyphonic plucked-string synthesizer built from the ground up in C++20 for the Raspberry Pi. It is both a production-ready instrument and a documented engineering journey — each chapter of the repository captures a distinct technical milestone, from a raw ALSA sine wave to a full-featured synthesizer with a real-time browser interface.

This project covers the full vertical stack of embedded audio: low-level **ARM memory barriers** and **lock-free thread synchronization**, physical modeling DSP, and a **WebSocket-driven web UI** with live spectrum analysis and waveguide visualization.

---

## Technical Highlights

* **Real-time Audio Engine** — ALSA PCM with a dedicated `SCHED_FIFO` priority-80 audio thread for deterministic, low-latency output.
* **Physical Modeling** — Karplus-Strong waveguide synthesis with tuning-compensated feedback gain, configurable pluck and pickup positions, and a DC blocker.
* **Thread Safety on ARM** — C++ atomics with explicit `acquire`/`release` ordering across the MIDI, audio, and web threads, accounting for ARM's weak memory model.
* **Advanced DSP**
  * Zero-Delay Feedback State Variable Filter (SVF) with MIDI CC keytracking
  * Master bus: Stereo Chorus, Freeverb reverb, and Ping-Pong Delay
  * Sample-accurate parameter smoothing to eliminate zipper noise
  * TPDF dithering for 16-bit output paths; float passthrough for 24/32-bit devices
* **Full-Stack Control** — Browser UI with real-time FFT spectrum analyzer, live waveguide state display, RMS/peak meter, preset management, and multi-client WebSocket sync.
* **Plug-and-Play Hardware** — Automatic USB audio device discovery with runtime sample-rate and bit-depth negotiation. MIDI controllers auto-connect on startup and hotswap without restarting.

---

## Development Journey

The repository is organized as a series of chapters. Each directory is a self-contained, buildable program with its own documentation covering the engineering decisions and challenges for that stage.

### [Chapter 09 — Plug and Play (Final)](./chapters/09_plug_and_play)
The complete synthesizer: full DSP engine, browser UI, and automatic hardware configuration.

### All Chapters

| # | Title | Key Focus |
| :---: | :--- | :--- |
| [01](./chapters/01_basic_sine) | Basic Sine | ALSA PCM fundamentals — buffer management, hardware params, xrun recovery |
| [02](./chapters/02_midi_monitor) | MIDI Monitor | Asynchronous MIDI event handling via the ALSA sequencer |
| [03](./chapters/03_simple_midi_synth) | Simple Synth | Bridging MIDI input to real-time audio output |
| [04](./chapters/04_poly_midi_synth) | Poly MIDI Synth | **Thread safety & atomics** — solving race conditions on ARM's weak memory model |
| [05](./chapters/05_pluck_synth) | Pluck Synth | **Physical modeling** — Karplus-Strong waveguide synthesis |
| [06](./chapters/06_pluck_synth_cc) | CC Control | Mapping MIDI CC messages to synthesis parameters with log/exp scaling |
| [07](./chapters/07_pluck_synth_effects) | Effects Bus | **DSP** — Zero-Delay SVF, Freeverb reverb, Chorus, parameter smoothing |
| [08](./chapters/08_web_ui) | Web UI | uWebSockets integration, live FFT spectrum, waveguide visualization |
| [09](./chapters/09_plug_and_play) | Plug and Play | **Systems integration** — auto device discovery, hotswap, presets |

---

## Hardware

* **Raspberry Pi 5** running Pi OS Lite
* Any class-compliant **USB Audio Interface** (Apple USB-C dongle, Focusrite Scarlett, etc.)
* Any class-compliant **USB MIDI controller**

The final program auto-discovers the audio device at runtime, so simply plug in USB audio and MIDI devices after running the program.

---

## Setup & Build

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
git clone <repo-url>
cd synth
mkdir build && cd build
cmake ..
ninja
```

All executables land in `build/bin/`.

### 6. Run

```bash
./bin/09_plug_and_play
```

The program will auto-connect any plugged-in MIDI controllers and scan for a USB audio output. Open `http://<hostname>.local:9002` in a browser on the same network to access the control dashboard.

---

**Author:** Ken Kobayashi
