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
)
```

## Web Server Structure

This web server system is structured as a high-performance **Embedded Linux Appliance**, designed to provide real-time control and visualization for a synthesizer without compromising the timing of the audio engine. The architecture relies on strict thread isolation, using the `uWebSockets` library to manage network communication on a dedicated low-priority thread while high-priority audio processing continues uninterrupted.

### Web Thread

The system has 2 threads that manage the audio and the MIDI. A web thread is added to manage websockets. The thread runs a `uWebSockets` event loop that serves the HTML interface, manages WebSocket connections, and handles JSON serialization/deserialization.

First, the `web_server` class loads `/src/web/index.html` into memory. `start.(int port)` spawns a new thread and enteres the `run()` event which kicks off the uWebSockets event loop.

When a user connects to the servers port on the Pi's IP address, the server sends the `index.html` with a no-store header so no caching is done, so the browser always uses the most up to date version of the UI for dev purposes.

Once the page loads, the JS in the html opens a persistent WebSocket connection, which enables high speed visualization and realtime control.

The `.ws` config defines how the server reacts to a client connecting:

- `.open`: when a client connects, they are added to a set of clients, and calls `send_initial_state()` that syncs init data.
- `.message`: When a slider is moved in the UI, the JS sends JSON to the server, which is passed to `MsgDispatcher`.
- `.close`: Cleans up the client list to stop the connection.

The `run()` method also configures `us_timer` to regularly (every `1000 / Config::UI_UPDATES_PER_SECOND` ms) process FFT data and send to clients.

The `stop()` method uses `loop->defer()` to close all sockets properly.

### Outbound Data Flow (Synth → Browser)
Telemetry and state updates are pushed to the browser using a "Typed Message" pattern:
1.  **Trigger:** An event (e.g., a new audio meter reading, spectrum info, parameter change) occurs in a high-priority thread.
2.  **Encapsulation:** The data is packed into a lightweight struct (e.g., `MeterMsg` or `ParamMsg`).
3.  **Deferral:** The struct is moved into a lambda and passed to the web thread via `loop->defer`.
4.  **Serialization:** On the web thread, the message is converted to JSON using `JsonMsg`.
5.  **Broadcast:** The server sends the JSON string to all connected clients.


### Messages

The JSON messages are handled without a library. `msg_builder.hpp` provides all the functions to add elements to the JSON depending on the data that is passed. `messages.hpp` descibes the types of packages that are sent, providing the name of the message and the data included in it. This message is then broadcast by the web thread using `loop->defer`, which tells the web thread to send the message whenever it has time.

### Inbound Data Flow (Browser → Synth)
Control messages from the UI are processed using an optimized dispatching system:
1.  **Client Input:** The browser's JavaScript sends a JSON string (e.g., a `set_param` command) through the WebSocket.
2.  **Dispatching:** The `MsgDispatcher` extracts the "type" field by expecting it to be the first key in the JSON, then routes the raw string to the appropriate handler.
3.  **Parsing:** The handler uses `MsgParser` to extract specific integer or float values (like parameter IDs and slider values) without a full JSON library parse.
4.  **Application:** The values are applied to the `SynthParams`.
5.  **Synchronization:** The updated parameter is broadcast back to all clients so that multiple open UI instances stay in sync.

## Audio Visualization

Three forms of audio visualization were added to the UI: a RMS meter, waveguide display, and a EQ style spectrum visualizer.

The refresh rate of these elements can be set in `config.hpp` with the `UI_UPDATES_PER_SECOND` value. This value is used in `audio.cpp` to determine how long to accumulate data before sending data to the web UI.

```cpp
meter_interval = static_cast<int>(sample_rate / period_size / 30);
```

### RMS Meter

The audio thread processes each sample and updates 4 local accumulartors:

```cpp
for (size_t i = 0; i < period_size; ++i) {
        meter_rms_l += mix_l[i] * mix_l[i];
        meter_rms_r += mix_r[i] * mix_r[i];
        meter_peak_l = std::max(meter_peak_l, std::abs(mix_l[i]));
        meter_peak_r = std::max(meter_peak_r, std::abs(mix_r[i]));
}
```

Once the `meter_interval` is reached, the mean is calculated and sent to the UI:

```cpp
float total = static_cast<float>(period_size * meter_interval);
on_meter(std::sqrt(meter_rms_l / total),
        std::sqrt(meter_rms_r / total),
        meter_peak_l,
        meter_peak_r);
```

This `on_meter()` funcion is a callback defined in `main.cpp` linked to the web server:

```cop
audio.on_meter = [&web](float rl, float rr, float pl, float pr) {
    web.broadcast(MeterMsg {rl, rr, pl, pr});
};
```

### Waveguide Display

As the synth uses a digital waveguide, the state of the string can actually be shown. Although its not super useful on its own at 30 fps, it is useful for understanding the pluck positiona and pickup position parameters of the synth.

The synth normally doesn't process the entire string, so a `Pluck::snapshot` function is added to expose the entire string's value. Furthermore, since the synth has 8 voices, the UI must choose one string to visualize, which is the most recently played one.

After the string is selected, the `half_delay_len` is identified, and the active points are resampled to 128 points to keep the length of the string in the UI consistent. I experimented with representing the actual length of the string, but I found it made the visualization very small and difficult to understand.

Every time a UI update needs to happen, the audio thread snapshots the string state and executes a callback (on_waveguide) to hand the data to the web thread. The web thread converts the data to JSON and sends it to all clients.

### Spectrum

The spectrum uses the `pffft` library for ffts. It is imported as a submodule:

```bash
git submodule add https://github.com/marton78/pffft.git deps/pffft
git submodule update --init --recursive
```

The PFFFT library should be optimized for SIMD on ARM CPU's which the RasPi has.

The first step of the fft analysis to to collect the audio data. The `fft_accumulator` class is a ring buffer that the audio loop writes mono sample data into.

```cpp
fft_acc.write((mix_l[i] + mix_r[i]) * 0.5f);
```

The size of the accumulator is set in `config.hpp` with `FFT_ACC_SIZE`. This is set to double the actual FFT sample size to be sure, but must be larger than the actual sample size at minimum.

The actual FFT processing is done in `fft_processor.cpp`, which is a wrapper for the PFFFT library.

This wrapper first applies a Blackman-Harris window, then executes a real forward FFT to produce 2048 frequency bins (assuming 4096 samples). Each bin is complex, so they are all converted to magnitudes. A logarthmic curve is applied so the low frequencies occupy more space. The output bins are also reduced to `FFT_OUT_BINS` in `config.hpp` to save packet sizes and enable the log scaling.


## Parameter Synchronization

As the parameters can be modified in many places now, all sources must be synchronized.

When a MIDI message or a web UI changes a value, the changes are broadcasted to all connected web UI clients.

```cpp
// MIDI thread -> web: broadcast param change to all clients
midi.on_param_change =
        [&web](SynthParams::ParamId id, float norm, float val, const char *name, const char *unit) {
                web.broadcast(ParamMsg {id, norm, val, name, unit});
        };

// web thread -> synth: apply slider change, echo to all clients for multi-client sync
dispatcher.on("set_param", [&params, &web](std::string_view msg) {
        int id_int  = MsgParser::extract_int(msg, "id");
        float value = MsgParser::extract_float(msg, "value");

        if (id_int < 0 || id_int >= static_cast<int>(SynthParams::ParamId::COUNT)) return;

        auto id = static_cast<SynthParams::ParamId>(id_int);
        params.set_param(id, value);

        auto d = params.descriptor(id);
        web.broadcast(ParamMsg {id, value, params.get_value(id), d.name, d.unit});
});
```