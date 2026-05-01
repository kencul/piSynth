# Source

This directory contains the synthesizer source code. It is a polyphonic plucked-string waveguide synthesizer with a browser-based control and visualization interface, automatic USB audio and MIDI device discovery, and hotswap support.

## Build & Run

```bash
mkdir build && cd build
cmake ..
ninja
./bin/synth
```

Launching will auto-connect any plugged-in MIDI controllers, scan for a USB audio output, and start a web server. Run `hostname` in the Pi console to find the hostname, then open `http://<hostname>.local:9002` in a browser on the same network.

## Configuration

Adjust voice parameters, effects settings, and web UI refresh rate in [config.hpp](config.hpp):

```cpp
inline constexpr int   MAX_VOICES             = 8;
inline constexpr float KILL_MS                = 1.5f;    // voice steal fade time
inline constexpr float FILTER_KEYTRACK        = 0.8f;    // filter cutoff tracking
inline constexpr int   FFT_SIZE               = 4096;    // FFT window size
inline constexpr int   FFT_ACC_SIZE           = 8192;    // FFT accumulation buffer size
inline constexpr int   FFT_OUT_BINS           = 512;     // downsampled bins for UI
inline constexpr int   UI_UPDATES_PER_SECOND  = 60;      // web refresh rate
inline constexpr int   UI_PORT                = 9002;    // web server port
```

## Source Structure

| Directory | Contents |
| :--- | :--- |
| [adsr/](adsr/) | Gate-based ADSR envelope with sample-accurate smoothing |
| [audio/](audio/) | ALSA audio engine — device discovery, format negotiation, SCHED_FIFO thread |
| [common/](common/) | `SynthParams` (atomic parameter store, CC mapping, presets), `SmoothedValue` |
| [effects/](effects/) | `MasterBus` combining Chorus, Freeverb reverb, and Ping-Pong delay; Zero-Delay SVF filter |
| [fft/](fft/) | Blackman-Harris windowed FFT accumulator and processor for the spectrum analyzer |
| [midi/](midi/) | ALSA sequencer MIDI reader — auto-connect, hotswap, CC and note event handling |
| [osc/](osc/) | Karplus-Strong waveguide oscillator with tuning-compensated feedback and DC blocker |
| [voice/](voice/) | `Voice`, `VoiceManager`, lock-free `RingBuffer`, `NoteEvent` |
| [web/](web/) | uWebSockets server, `index.html`, JSON message types and dispatcher |

## Features

### Browser Interface
* **Parameter sliders**: All synth parameters exposed as sliders. Changes are broadcast to all connected clients so multiple open tabs stay in sync.
* **Preset panel**: Save, load, and delete named presets. A factory default reset button restores all parameters to their initial values.
* **Device display**: Active audio output and connected MIDI controller names shown live as devices connect or disconnect.
* **RMS Meter**: Stereo RMS and peak meter updated at `UI_UPDATES_PER_SECOND`.
* **Waveguide Display**: Digital waveguide state resampled to 128 points for visualization. Shows the effect of pluck and pickup position in real time.
* **Spectrum Analyzer**: EQ-style spectrum using a 4096-sample Blackman-Harris windowed FFT, log-scaled and downsampled to `FFT_OUT_BINS` bins.

### Audio & Hardware
* **Sample Rate**: Prefers `48000Hz`, falls back to `44100Hz`, then nearest available.
* **Bit Depth**: Sends `float` if the device supports 32-bit or 24-bit PCM; falls back to `S16_LE` with TPDF dithering in software.
* **Device**: Any USB audio device with PCM playback output — selected automatically via the `USB-Audio` ALSA driver string.
* **Scheduling**: Audio thread runs at SCHED_FIFO priority 80 via `cap_sys_nice` granted by `setcap` during the build.

## CC Parameter Control

Parameters are controlled via MIDI CC messages. Per-sample and per-block parameter smoothing eliminates zipper noise during real-time adjustment.

| CC | Parameter        | Range             | Scale  | Effect |
|----|------------------|-------------------|--------|--------|
| 14 | Master Gain      | 0.0 – 1.0         | Power  | Master |
| 15 | Decay Time       | 10ms – 15000ms    | Log    | Voice  |
| 16 | Pluck Pos        | 0.0 – 0.5         | Linear | Voice  |
| 17 | Pickup Pos       | 0.05 – 0.5        | Linear | Voice  |
| 18 | Attack Time      | 0.1ms – 50ms      | Log    | Voice  |
| 19 | Release Time     | 1ms – 1000ms      | Log    | Voice  |
| 20 | Filter Cutoff    | 20Hz – 18000Hz    | Exp    | Filter |
| 21 | Filter Resonance | 0.0 – 1.0         | Linear | Filter |
| 22 | Chorus Rate      | 1Hz – 5Hz         | Log    | Chorus |
| 23 | Chorus Depth     | 0.0 – 2.0         | Linear | Chorus |
| 24 | Chorus Mix       | 0.0 – 1.0         | Linear | Chorus |
| 25 | Delay Time       | 1ms – 1000ms      | Power  | Delay  |
| 29 | Delay Feedback   | 0.0 – 0.95        | Linear | Delay  |
| 1  | Delay Mix        | 0.0 – 1.0         | Linear | Delay  |
| 26 | Reverb Room Size | 0.0 – 1.0         | Linear | Reverb |
| 27 | Reverb Cutoff    | 1kHz – 10kHz      | Exp    | Reverb |
| 28 | Reverb Mix       | 0.0 – 1.0         | Linear | Reverb |

## Boot on Launch

To have the synth start automatically when the Pi boots:

1. **Install the binary:**
   ```bash
   # From the build directory
   sudo cp bin/synth /usr/local/bin/pi-synth
   ```

2. **Grant real-time capabilities:**
   ```bash
   sudo setcap cap_sys_nice+ep /usr/local/bin/pi-synth
   ```
   > Copying the binary strips file capabilities, so this re-applies them on the installed copy. This allows the audio thread to set its own SCHED_FIFO priority at runtime.

3. **Install the service file:**
   ```bash
   sudo cp ../systemd/pi-synth.service /etc/systemd/system/
   ```

   Then open it and set your username:
   ```bash
   sudo nano /etc/systemd/system/pi-synth.service
   ```
   Replace `YOUR_USERNAME_HERE` on the `User=` and `Group=` lines with your system username (`whoami` to find it). Save and exit with Ctrl+O, Enter, Ctrl+X.

4. **Enable and start:**
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable --now pi-synth.service
   ```

5. **Verify:**
   ```bash
   sudo systemctl status pi-synth.service
   ```

The synth will now launch on every boot. Connect an audio and MIDI device, and optionally open the web UI with no command line interaction required.
