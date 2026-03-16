# 04 Poly MIDI Synth

A polyphonic synth that plays a sine wave from a MIDI input. Has ADSR enveloping for each voice.

## Usage 

Build and run:
```bash
cd build
cmake ..
ninja
./bin/05_karplus_strong_synth
```

Launching will open the audio device and read MIDI controller based on values in `config.hpp`.

From there it responds immediately to your keyboard. Press a key and the sine tone plays at that pitch. Ctrl+C shuts everything down cleanly.

To change the audio device or the MIDI source, edit `config.hpp`

```cpp
inline constexpr const char *AUDIO_DEVICE = "hw:UR22mkII";
inline constexpr const char *MIDI_DEVICE  = "KOMPLETE KONTROL";
```

Also adjust the number of voices and ADSR values in `config.hpp`

```cpp
inline constexpr int MAX_VOICES = 4;

// ADSR defaults
inline constexpr float DEFAULT_ATTACK  = 10.0f;  // ms
inline constexpr float DEFAULT_DECAY   = 100.0f; // ms
inline constexpr float DEFAULT_SUSTAIN = 0.7f;   // level 0-1
inline constexpr float DEFAULT_RELEASE = 300.0f; // ms
```

## Structure

- **Startup Sequence**

```
main()
├── creates RingBuffer<NoteEvent, 64>
├── creates AudioEngine(event_queue)
│     └── creates VoiceManager
│           └── creates Voice[16]
│                 ├── Oscillator
│                 └── ADSR
├── creates MidiReader(event_queue)
├── audio.open()  → configures ALSA PCM device
├── midi.open()   → connects to ALSA sequencer
├── audio.start() → launches audio thread
└── midi.start()  → launches MIDI thread
```

- **Realtime Data Flow**

```
Physical key press on M32 (MIDI controller)
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
        └── CC      → logged only (not yet used)
        ↓
RingBuffer<NoteEvent, 64>   ← MIDI thread writes here
        ↓                      audio thread reads here
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
        │         → envelope.trigger() → Stage::Attack
        │         → osc.set_frequency(hz)
        │         → velocity_gain = velocity / 127.0f
        │
        └── NoteOff
              find_voice(note)
              voice.release()
                → envelope.release() → Stage::Release
                → active stays true until envelope idles
        ↓
voice_manager.process(buf, frames, channels)
        │
        │  float mix[PERIOD_SIZE] = {}
        │
        │  for each active voice:
        │    osc.process(tmp, frames)
        │      → sine samples in [-1.0, 1.0]
        │    for each frame:
        │      envelope_gain = envelope.process()
        │        → Attack:  level += attack_rate
        │        → Decay:   level -= decay_rate
        │        → Sustain: level unchanged
        │        → Release: level -= release_rate
        │        → Kill:    level -= kill_rate (fixed ramp over KILL_SAMPLES)
        │        → Idle:    level = 0.0
        │      mix[i] += tmp[i] * VOICE_GAIN * velocity_gain * envelope_gain
        │
        │  for idle voices:
        │    if pending note exists → trigger it (frequency/velocity applied at silence)
        │    else → deactivate voice
        │
        │  for each frame:
        │    clamp mix[i] to [-1.0, 1.0]
        │    scale to 24-bit value in S32_LE container
        │    write to buf[i * channels + ch] for each channel
        ↓
snd_pcm_writei(handle, buf, period_size)
        ↓
ALSA PCM driver (kernel) → ring buffer → hardware DMA
        ↓
UR22 MkII DAC (USB Audio Interface)
        ↓
Audio output
```

## Code Breakdown

This program fleshes out the previous one, by implementing polyphony: pressing multiple keys plays them at the same time. Clicking sounds when notes are released are solved through an ADSR envelope.

These two solve the basic issues with the previous simple version, setting the foundations to get into more creative aspects of synth design.

### Issues:

Voice gain staging was difficult to tune. Each voice should be louder when less voices are playing to try to make sure the overall volume is consistent. I explored many options, such as linear gain staging, smoothing out the gain staging, and square root gain staging with soft saturation. However, I didn't like how any of the solutions sounded, so there is no gain staging. Instead, each voice is `1/voice` amplitude loud. Thus, setting the max number of voices lower makes each voice louder. This means that I just crank up the gain on my interface when I have a high number of voices, and adjust the velocity of notes if I am playing many notes at once.

Voice stealing is implemented in a way that avoids clicks. When a new note arrives and no voices are free, the priority of the voice stolen is a releasing voice, then an active voice. In both cases, the voice enters a "kill envelope" where the voice fades in silence over `KILL_SAMPLES` (configured in `config.hpp`). The new note's data is stored in a `PendingNote` struct in the voice, and is applied once the voice reaches idle.