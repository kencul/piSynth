# 07 Pluck Synth Effects
A polyphonic pluck-string waveguide synthesizer with a per-voice Zero Delay SVF filter and a master effects bus including Chorus, Reverb, and Ping Pong Delay.

## Usage
Build and run:
```bash
cd build
cmake ..
ninja
./bin/07_pluck_synth_effects
```

Launching will open the audio device and read MIDI controller based on values in `config.hpp`. This version supports multiple MIDI devices simultaneously to accommodate more physical controllers.

To change the audio device or the MIDI sources, edit `config.hpp`:
```cpp
inline constexpr const char *AUDIO_DEVICE = "hw:A";
inline constexpr std::initializer_list<const char *> MIDI_DEVICES = {
    "KOMPLETE KONTROL",
    "Teensy MIDI"
};
```

Adjust the master effects settings and voice parameters in `config.hpp`:
```cpp
inline constexpr int   MAX_VOICES   = 8;
inline constexpr float KILL_MS      = 1.5f;     // ms: voice steal fade time
inline constexpr float FILTER_KEYTRACK = 0.8f;  // 0.0-2.0 cutoff tracking
```

## New Effects & Controls
Program 07 introduces a restructured processing chain. Each voice now contains its own **Zero Delay State Variable Filter (SVF)**, while global effects are processed on a dedicated **Master Bus**.

### Per-Voice Filter
* **SVF Filter**: A stable, accurate filter implemented for each voice. It includes **Keytracking**, which scales the cutoff frequency based on the note pitch so that different octaves maintain consistent timbre.

### Master Bus Effects
* **Chorus**: A stereo effect using dual LFO-modulated delay lines. The left and right channels use different modulation rates and base delay offsets to create maximum stereo width.
* **Freeverb**: A lush reverb implementation using 8 parallel comb filters followed by 4 serial allpass filters. It provides controls for room size, damping (cutoff), and dry/wet mix.
* **Ping Pong Delay**: A stereo delay where reflections bounce between the left and right channels. It features per-sample delay time smoothing to create smooth pitch-shifting "Doppler" effects when adjusted.

## CC Parameter Control
Parameters are controlled via MIDI CC messages. Per-sample and per-block **Parameter Smoothing** has been implemented for all CC inputs to eliminate "zipper noise" and clicks during real-time adjustment.

| CC | Parameter        | Range             | Scale | Effect |
|----|------------------|-------------------|-------|--------|
| 14 | Master Gain      | 0.0 – 1.0         | Power | Master |
| 15 | Decay Time       | 10ms – 15000ms    | Log   | Voice  |
| 16 | Pluck Pos        | 0.0 – 1.0         | Linear| Voice  |
| 17 | Pickup Pos       | 0.05 – 1.0        | Linear| Voice  |
| 18 | Attack Time      | 0.1ms – 50ms      | Log   | Voice  |
| 19 | Release Time     | 1ms – 1000ms      | Log   | Voice  |
| 20 | Filter Cutoff    | 20Hz – 18000Hz    | Exp   | Filter |
| 21 | Filter Resonance | 0.0 – 1.0         | Linear| Filter |
| 22 | Chorus Rate      | 1Hz – 5Hz         | Log   | Chorus |
| 23 | Chorus Depth     | 0.0 – 2.0         | Linear| Chorus |
| 24 | Chorus Mix       | 0.0 – 1.0         | Linear| Chorus |
| 25 | Delay Time       | 1ms – 1000ms      | Power | Delay  |
| 29 | Delay Feedback   | 0.0 – 0.95        | Linear| Delay  |
| 1  | Delay Mix        | 0.0 – 1.0         | Linear| Delay  |
| 26 | Reverb Room Size | 0.0 – 1.0         | Linear| Reverb |
| 27 | Reverb Cutoff    | 1kHz – 10kHz      | Exp   | Reverb |
| 28 | Reverb Mix       | 0.0 – 1.0         | Linear| Reverb |



## DAC & Audio Configuration

The system continues to target the **Apple USB-C dongle** with the following hardware settings:
* **Format**: `S16_LE`
* **Sample Rate**: `48000Hz`
* **Device Name**: `hw:A`
* **Scheduling**: Automatically grants `SCHED_FIFO` real-time priority (priority 80) via `setcap` in the build process to ensure low-latency performance.

## Startup Sequence

```
main()
├── creates RingBuffer<NoteEvent, 64>
├── creates SynthParams (initializes CC mappings and default values)
├── creates AudioEngine(event_queue, params)
│     └── creates VoiceManager(params)
│           ├── creates Voice[8]
│           │     ├── Pluck (Plucked-string model)
│           │     ├── SVF   (Zero Delay State Variable Filter)
│           │     └── ADSR  (Gate-based envelope)
│           └── creates MasterBus(params)
│                 ├── Chorus    (Stereo LFO delay)
│                 ├── Freeverb  (Comb/Allpass reverb)
│                 └── PingPong  (Cross-feedback delay)
├── creates MidiReader(event_queue, params)
├── audio.open()  → configures ALSA PCM (S16_LE, 48000Hz)
├── midi.open()   → connects to multiple devices in config.hpp
├── audio.start() → launches high-priority audio thread
└── midi.start()  → launches MIDI polling thread
```

## Realtime Data Flow

```
Physical MIDI Event (Keyboard or CC knob)
        ↓
USB MIDI packet → kernel ALSA driver
        ↓
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
MIDI THREAD (MidiReader::midi_loop)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
poll() wakes on sequencer activity
        ↓
snd_seq_event_input() → drain events
        ↓
handle_event()
        ├── NoteOn/Off → event_queue.push({ type, note, velocity })
        └── CC Message → params.handle_cc(cc, value)
                           → normalize 0–127 to 0.0–1.0
                           → apply scaling (Log/Exp/Linear)
                           → store in Atomic float array
        ↓
RingBuffer<NoteEvent, 64>   ← MIDI thread writes here
        ↓                      audio thread reads here
SynthParams (Atomics)       ← MIDI thread stores targets
        ↓                      audio thread loads targets
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
AUDIO THREAD (AudioEngine processing)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
1. DRAIN MIDI QUEUE
   while (event_queue.pop()) → voice_manager.handle(event)
     → updates voice states (Trigger/Release/Steal)

2. PROCESS VOICES (VoiceManager::process)
   for each active voice:
     a. Load param targets (Cutoff, Resonance, etc.)
     b. osc.process() → Generate plucked string sample
     c. filter.process() → Apply per-voice SVF Filter
     d. adsr.process() → Calculate envelope gain
     e. Accumulate into mix_l and mix_r buffers

3. PROCESS MASTER EFFECTS (MasterBus::process)
   a. Chorus    → Apply dual-LFO modulated delay
   b. PingPong  → Apply cross-stereo delay with smoothing
   c. Freeverb  → Apply Schroeder/Moorer reverb
   d. Smoothed Gain → Apply master volume with sample-accurate ramp

4. FINAL OUTPUT
   a. Soft Clip → tanh(mix * DRIVE) / DRIVE (Bus limiting)
   b. Scale     → Convert float to S16_LE (INT16_MAX)
   c. ALSA      → snd_pcm_writei() to hardware DMA
```

## Code Breakdown
 
This program implements a web UI for the synth through web sockets.

### uWebSockets

The [uWebSockets](https://github.com/uNetworking/uWebSockets) library is used to handle websockets for the UI. It is a relatively easy to use library made in C++.

The library is added as a git submodule in the repo:

```bash
git submodule add https://github.com/uNetworking/uWebSockets.git deps/uWebSockets
```

This clones the uWebSockets repo into the `/deps` directory. However, the library depends on the `uSockets` library, so it must also be pulled:

```bash
git submodule update --init --recursive
```

The repo doesn't come with any CMakeLists.txt file, so it must be created in the project. This has the advantage of being able to only include features that I need. For instance, I don't need cryptography features, so I can omit SSL. The library is then grouped into a library file:

```bash
file(GLOB USOCKETS_SOURCES
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/uSockets/src/*.c
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/uSockets/src/eventing/*.c
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/uSockets/src/crypto/*.c
)

# exclude SSL backends
list(FILTER USOCKETS_SOURCES EXCLUDE REGEX ".*openssl.*")
list(FILTER USOCKETS_SOURCES EXCLUDE REGEX ".*wolfssl.*")
list(FILTER USOCKETS_SOURCES EXCLUDE REGEX ".*boringssl.*")

add_library(uSockets STATIC ${USOCKETS_SOURCES})

target_include_directories(uSockets PUBLIC
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/uSockets/src
)
target_compile_definitions(uSockets PUBLIC LIBUS_NO_SSL=1)

...

target_include_directories(08_web_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/deps/uWebSockets/src
)

target_link_libraries(08_web_ui PRIVATE
    ...
    uSockets
    z
)
```

The library uses zlib to compress messages, so zlib library must be available in the system:

```bash
sudo apt-get update
sudo apt-get install zlib1g-dev
```