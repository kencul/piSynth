# 03 Simple MIDI Synth

A simple mono synth that plays a sine wave from a MIDI input.

## Usage 

Build and run:
```bash
cd build
cmake ..
ninja
./bin/03_midi_synth
```

Launching will open the audio device and read MIDI controller based on values in `config.hpp`.

From there it responds immediately to your keyboard. Press a key and the sine tone plays at that pitch. Ctrl+C shuts everything down cleanly.

To change the audio device or the MIDI source, edit `config.hpp`

```cpp
inline constexpr const char *AUDIO_DEVICE = "hw:UR22mkII";
inline constexpr const char *MIDI_DEVICE  = "KOMPLETE KONTROL";
```

## Structure

```
03_midi_synth/
├── config.hpp              # all constants — sample rate, period, devices, amplitude
├── main.cpp                # entry point, owns shared state, signal handling, wires classes
├── osc/
│   ├── osc.hpp             # Oscillator interface
│   └── osc.cpp             # sine generation, phase accumulator
├── audio/
│   ├── audio.hpp           # AudioEngine interface
│   └── audio.cpp           # ALSA PCM, audio thread, calls oscillator
└── midi/
    ├── midi.hpp            # MidiReader interface
    └── midi.cpp            # ALSA sequencer, poll loop, event handling, updates shared state
```

**Data flow**

```
MIDI Controller (hardware)
        ↓
    kernel snd-usb-audio driver
        ↓
    ALSA sequencer (kernel)
        ↓
    MidiReader (midi thread)
        │
        │  poll() blocks up to 50ms on sequencer file descriptors
        │  wakes on MIDI event or timeout
        │
        │  on timeout ─────────────────────────→ recheck running flag → loop
        │
        │  on event:
        │  snd_seq_event_input_pending(seq, 1)  ← fetch from kernel into local buffer
        │  snd_seq_event_input()                ← drain one event at a time
        │  handle_event()
        │       Note On  → frequency.store(midi_to_hz(note))
        │                  notes_active.fetch_add(1)
        │       Note Off → notes_active.fetch_sub(1)
        │       CC       → (logged, not yet used)
        ↓
    ┌────────────────────────────────────────┐
    │  shared state (owned by main)          │
    │  std::atomic<double>       frequency   │
    │  std::atomic<unsigned int> notes_active│
    └────────────────────────────────────────┘
        ↓
    AudioEngine (audio thread)
        │
        │  frequency.load()    → if changed, osc.set_frequency()
        │  notes_active.load() → if > 0, osc.process() else fill silence
        │  snd_pcm_writei()    → send period buffer to ALSA driver
        │  on xrun → snd_pcm_recover() → continue
        ↓
    ALSA PCM driver (kernel)
        ↓
    UR22 MkII (hardware)
        ↓
    Audio output
```

## Code Breakdown

The fundamental blocks of this program is from the previous two, where the Pi outputs a sine wave to the interface, and receive MIDI messages from a MIDI controller. Bridging the gap so the controller controls the synth had many challenges

**1. Two blocking calls, one thread**

`snd_pcm_writei` and `snd_seq_event_input` both block indefinitely. Running them sequentially in one thread means whichever runs first starves the other, as audio stops while waiting for MIDI, or MIDI goes unread while writing audio. Instead each process is separated into two threads, one per blocking call.

```cpp
void AudioEngine::start() {
    running.store(true);
    thread = std::thread(&AudioEngine::audio_loop, this);
}

void MidiReader::start() {
    running.store(true);
    thread = std::thread(&MidiReader::midi_loop, this);
}
```

Each class launches each blocking loop on its own independent thread so both run concurrently.

---

**2. Shared variables between threads**

The MIDI thread needs to tell the audio thread the frequency changed. A plain `double` shared between threads is a race condition.

`std::atomic<double>` guarantees reads and writes are indivisible. On ARM64 this compiles to a single CPU instruction with no locking so its efficient.

```cpp
// owned in main, passed by reference to both classes
std::atomic<double>       frequency    { 0.0 };
std::atomic<unsigned int> notes_active { 0   };
```

```cpp
// MIDI thread writes
frequency.store(midi_to_hz(ev->data.note.note));

// audio thread reads
double new_freq = frequency.load();
```

---

**5. Redundant oscillator updates**

Calling `osc.set_frequency()` every period recalculates `phase_inc` via `pow` and division on every audio loop iteration, even when the frequency hasn't changed.

This can me fixed by caching the last frequency in the audio loop and only updating the oscillator when the value actually changes.

```cpp
double current_freq = 0.0;

while (running.load()) {
    double new_freq = frequency.load();

    // pow only called when frequency genuinely changes
    if (new_freq != current_freq) {
        osc.set_frequency(new_freq);
        current_freq = new_freq;
    }
    ...
}
```

---

**6. Clean shutdown on Ctrl+C**

Without signal handling, Ctrl+C sends `SIGINT` which kills the process immediately. This bypasses `stop()`, skipping `snd_pcm_drain`, and leaving the ALSA device in an undefined state. Instead, I catch the signal, set a flag, let the main thread exit the wait loop and call `stop()` on both classes normally.

```cpp
static std::atomic<bool> should_quit { false };

static void on_signal(int) {
    should_quit.store(true);
}

std::signal(SIGINT,  on_signal);
std::signal(SIGTERM, on_signal);

while (!should_quit.load())
    pause();

audio.stop();
midi.stop();
```

`pause()` sleeps the main thread until any signal arrives, so it consumes no CPU while waiting.

---

**7. Safe repeated stop calls**

`stop()` is called by the destructor, but could also be called manually before the object goes out of scope. Calling `thread.join()` on an already-joined thread is undefined behaviour and crashes. `joinable()` returns false after a thread has been joined or was never started, making `stop()` safe to call any number of times.

```cpp
void AudioEngine::stop() {
    running.store(false);
    if (thread.joinable())  // false if already joined or never started
        thread.join();
    ...
}
```

The destructor also calls `stop()` so the thread is always cleaned up even if the object goes out of scope unexpectedly.