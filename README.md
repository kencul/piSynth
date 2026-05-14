# PiSynth

**PiSynth** is a polyphonic plucked-string synthesizer written in C++20 for the Raspberry Pi. It is a working instrument and a documented engineering project, where each chapter of the repository is a distinct technical milestone, starting from a raw ALSA sine wave and building up to a full synthesizer with a real-time browser interface.

---

## Technical Highlights

* **Real-time Audio Engine**: ALSA PCM with a dedicated `SCHED_FIFO` priority-80 audio thread for deterministic, low-latency output.
* **Physical Modeling**: Karplus-Strong waveguide synthesis with tuning-compensated feedback gain, configurable pluck and pickup positions, and a DC blocker.
* **Thread Safety on ARM**: C++ atomics with explicit `acquire`/`release` ordering across the MIDI, audio, and web threads, accounting for ARM's weak memory model.
* **Advanced DSP**:
  * Zero-Delay Feedback State Variable Filter (SVF) with MIDI CC keytracking
  * Master bus: Stereo Chorus, Freeverb reverb, and Ping-Pong Delay
  * Sample-accurate parameter smoothing to eliminate zipper noise
  * TPDF dithering for 16-bit output paths; float passthrough for 24/32-bit devices
* **Full-Stack Control**: Browser UI with real-time FFT spectrum analyzer, live waveguide state display, RMS/peak meter, preset management, and multi-client WebSocket sync.
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

## Credits
- [uWebSockets](https://github.com/uNetworking/uWebSockets) by Alex Hultman (Apache 2.0)
- [PFFFT](https://github.com/marton78/pffft) by Julien Pommier, fork maintained by marton78 (BSD-like)

This project is licensed under the [MIT License](LICENSE).

---

Built by [Ken Kobayashi](https://www.kenmusic.net/)
