# 06 Pluck Synth CC

A polyphonic pluck-string waveguide synthesizer driven by MIDI input with real-time CC parameter control.

## Usage

Build and run:
```bash
cd build
cmake ..
ninja
./bin/06_pluck_synth_cc
```

Launching will open the audio device and read MIDI controller based on values in `config.hpp`.

From there it responds immediately to your keyboard. Press a key and the plucked string sounds at that pitch. Ctrl+C shuts everything down cleanly.

To change the audio device or the MIDI source, edit `config.hpp`:
 
```cpp
inline constexpr const char *AUDIO_DEVICE = "hw:A";
inline constexpr const char *MIDI_DEVICE  = "KOMPLETE KONTROL";
```
 
Adjust voices and kill time in `config.hpp`:
 
```cpp
inline constexpr int   MAX_VOICES   = 8;
inline constexpr float KILL_MS      = 1.5f;     // ms: voice steal fade time
```
 
Adjust the parameter ranges in `config.hpp`:
 
```cpp
inline constexpr float MIN_DECAY_MS      = 10.0f;    // ms
inline constexpr float MAX_DECAY_MS      = 15000.0f; // ms
inline constexpr float MAX_ATTACK_TIME   = 50.0f;    // ms
inline constexpr float MAX_RELEASE_TIME  = 1000.0f;  // ms
inline constexpr float MIN_PICKUP_POS    = 0.05f;
```
 
Adjust the saturation ceiling:
 
```cpp
inline constexpr float SATURATION_DRIVE = 1.0f; // must be >= 1.0
```

## CC Parameter Control
 
Parameters are controlled via MIDI CC messages. Each parameter has a default value, a min/max range, and a scaling curve. All mappings and scaling live in `synth_params.cpp`.
 
| CC | Parameter    | Range                   | Scale  |
|----|--------------|-------------------------|--------|
| 14 | Master Gain  | 0.0 – 1.0               | Exp    |
| 15 | Decay Time   | 10ms – 15000ms          | Log    |
| 16 | Pluck Pos    | 0.0 – 1.0               | Linear |
| 17 | Pickup Pos   | 0.05 – 1.0              | Linear |
| 18 | Attack Time  | 0.1ms – 50ms            | Log    |
| 19 | Release Time | 1ms – 1000ms            | Log    |
 
Scaling curves map the 0–127 CC range to a normalized 0–1 value before it is mapped to the parameter range:
 
- **Linear**: proportional, good for spatial parameters like pluck and pickup position
- **Log**: compresses the low end, good for time-based parameters where small values need precision
- **Exp**: expands the low end, good for gain where most useful range is in the lower values
 
To add a new parameter, add it to the `ParamId` enum in `synth_params.hpp`, add a descriptor in the `SynthParams` constructor, and add a CC entry in `cc_map` in `synth_params.cpp`.

## DAC
 
This program targets the **Apple USB-C to 3.5mm Headphone Jack** dongle instead of the UR22 MkII used in the previous version. Several things change as a result:
 
**Format: `S32_LE` → `S16_LE`**

In `configure_device()` in `audio.cpp`:

```cpp
snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
```
 
The dongle does not support `S32_LE`. It offers `S24_3LE` (packed 3-byte) and `S16_LE`. `S16_LE` was chosen as it maps directly to `int16_t` with no manual byte packing required.
 
**Sample rate: 44100Hz → 48000Hz**
 
The dongle only supports 48000Hz. `Config::SAMPLE_RATE` is updated accordingly.
 
**Sample scale**
 
The 24-bit-in-32 scale factor is replaced with the 16-bit equivalent:
 
```cpp
// was: (1 << 23 - 1) * (1 << 8) for 24-bit in S32_LE container
inline constexpr double SAMPLE_SCALE = 32767.0; // INT16_MAX
```
 
**Buffer type**
 
The audio buffer changes from `std::vector<int32_t>` to `std::vector<int16_t>` in `audio.cpp`, and `VoiceManager::process()` is updated to write `int16_t` output accordingly.
 
**Device name**
 
```cpp
inline constexpr const char *AUDIO_DEVICE = "hw:A";
```
 
The short card ID `A` is used rather than the card number, as card numbers can change between reboots depending on what is plugged in.

## Structure
 
### **Startup Sequence**
 
```
main()
├── creates RingBuffer<NoteEvent, 64>
├── creates SynthParams
├── creates AudioEngine(event_queue, params)
│     └── creates VoiceManager(params)
│           └── creates Voice[8]
│                 ├── Pluck (Plucked-string model)
│                 └── ADSR  (release gate only)
├── creates MidiReader(event_queue, params)
├── audio.open()  → configures ALSA PCM device
├── midi.open()   → connects to ALSA sequencer
├── audio.start() → launches audio thread
└── midi.start()  → launches MIDI thread
```
 
### **Realtime Data Flow**
 
```
Physical key press or CC message on MIDI controller
        ↓
USB MIDI packet → kernel snd-usb-audio driver
        ↓
ALSA sequencer (kernel) → places event in sequencer queue
        ↓
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
MIDI THREAD
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
poll() wakes on sequencer file descriptor
        ↓
snd_seq_event_input_pending(seq, 1) → fetch from kernel
        ↓
snd_seq_event_input() → drain event
        ↓
handle_event()
        ↓
        ├── NoteOn  → event_queue.push({ NoteOn,  note, velocity })
        ├── NoteOff → event_queue.push({ NoteOff, note, 0       })
        └── CC      → params.handle_cc(cc, value)
                        → look up ParamId in cc_map
                        → apply_scale(value / 127.0, scale) → 0-1
                        → params[id].store(normalized)
        ↓
RingBuffer<NoteEvent, 64>   ← MIDI thread writes here
        ↓                      audio thread reads here
SynthParams atomics         ← MIDI thread stores here
        ↓                      audio thread loads here
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
AUDIO THREAD
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
snd_pcm_writei() completes previous period
        ↓
drain ring buffer:
while (auto ev = event_queue.pop())
    voice_manager.handle(ev)
        │
        ├── NoteOn
        │     allocate_voice()
        │       → free voice first
        │       → releasing voice second
        │       → steal oldest third
        │     if voice is active:
        │       voice.steal(note, hz, velocity)
        │         → pending = { note, hz, velocity }
        │         → envelope.kill() → Stage::Kill
        │     else:
        │       voice.trigger(note, hz, velocity)
        │         → amplitude = velocity/127 / sqrt(MAX_VOICES)
        │         → osc.set_frequency(hz)
        │         → osc.set_decay(60000 / params.DecayTime)
        │         → osc.trigger(params.PluckPos, params.PickupPos, amplitude)
        │             → seeds delay line with triangle at amplitude
        │         → envelope.set_attack(params.AttackTime)
        │         → envelope.trigger() → Stage::Attack
        │
        └── NoteOff
              voice.release()
                → envelope.set_release(params.ReleaseTime)
                → envelope.release() → Stage::Release
                → active stays true until envelope idles
        ↓
voice_manager.process(buf, frames, channels)
        │
        │  float mix_l[PERIOD_SIZE] = {}
        │  float mix_r[PERIOD_SIZE] = {}
        │
        │  for each active voice:
        │    osc.process(tmp, frames)
        │      → reads delay line at pickup position (interpolated)
        │      → feedback: (sample + prev) * feedback_gain written back
        │      → natural decay built into the feedback gain
        │    for each frame:
        │      envelope_gain = envelope.process()
        │        → Attack: level += attack_rate
        │        → Sustain: level = 1.0 (holds until release)
        │        → Release: level -= release_rate
        │        → Kill:    level -= kill_rate
        │        → Idle:    level = 0.0
        │      mix_l[i] += tmp[i] * envelope_gain * pan_left
        │      mix_r[i] += tmp[i] * envelope_gain * pan_right
        │
        │  for idle voices:
        │    if pending note exists → trigger it
        │    else → osc.clear(), voice.active = false
        │
        │  for each frame:
        │    tanh(mix[i] * SATURATION_DRIVE) / SATURATION_DRIVE
        │    multiply by params.MasterGain
        │    clamp to [-1.0, 1.0]
        │    scale to 16-bit value (INT16_MAX = 32767)
        │    write to buf[i * channels + ch] for each channel
        ↓
snd_pcm_writei(handle, buf, period_size)
        ↓
ALSA PCM driver (kernel) → ring buffer → hardware DMA
        ↓
Apple USB-C to 3.5mm DAC
        ↓
Audio output
```

## Code Breakdown
 
This version adds a CC parameter control layer on top of the waveguide synth from the previous version. The string model, voice architecture, and ADSR are unchanged.
 
### SynthParams
 
`SynthParams` is the shared parameter bus between the MIDI and audio threads. It owns:
 
- A normalized `atomic<float>` per parameter (0–1)
- A `ParamDescriptor` per parameter defining min, max, default, scaling curve, name, and unit
- A CC-to-ParamId map
 
The MIDI thread calls `handle_cc(cc, value)` which scales the 0–127 value to 0–1 and stores it atomically. The audio thread calls `value(id)` which loads the normalized value and maps it to the descriptor's min/max range. No locks are needed as the atomic store/load is the only synchronization point.
 
The descriptor's `default_value` is in end-value space (ms, not normalized). The constructor inverts it through the linear range to derive the correct starting normalized value.

Having the normalized value and descriptors sets up for the UI, exposing all the values and information it needs.