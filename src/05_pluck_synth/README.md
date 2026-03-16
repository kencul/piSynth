# 05 Pluck Synth

A polyphonic pluck-string model synthesizer driven by MIDI input.

## Usage

Build and run:
```bash
cd build
cmake ..
ninja
./bin/05_pluck_synth
```

Launching will open the audio device and read MIDI controller based on values in `config.hpp`.

From there it responds immediately to your keyboard. Press a key and the plucked string sounds at that pitch. Ctrl+C shuts everything down cleanly.

To change the audio device or the MIDI source, edit `config.hpp`:

```cpp
inline constexpr const char *AUDIO_DEVICE = "hw:UR22mkII";
inline constexpr const char *MIDI_DEVICE  = "KOMPLETE KONTROL";
```

Adjust voices, decay, and release in `config.hpp`:

```cpp
inline constexpr int   MAX_VOICES       = 8;
inline constexpr float DEFAULT_RELEASE  = 100.0f;  // ms: envelope release on note off
inline constexpr float DEFAULT_DECAY_MS = 30000.0f; // ms: KS string T60 decay time
inline constexpr float KILL_MS          = 1.5f;     // ms: voice steal fade time
```

Adjust the saturation ceiling:

```cpp
inline constexpr float SATURATION_DRIVE = 1.0f; // must be >= 1.0
```

## Structure

- **Startup Sequence**

```
main()
├── creates RingBuffer<NoteEvent, 64>
├── creates AudioEngine(event_queue)
│     └── creates VoiceManager
│           └── creates Voice[8]
│                 ├── Pluck (Plucked-string model)
│                 └── ADSR  (release gate only)
├── creates MidiReader(event_queue)
├── audio.open()  → configures ALSA PCM device
├── midi.open()   → connects to ALSA sequencer
├── audio.start() → launches audio thread
└── midi.start()  → launches MIDI thread
```

- **Realtime Data Flow**

```
Physical key press on MIDI controller
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
        │         → amplitude = velocity/127 / sqrt(MAX_VOICES)
        │         → osc.set_frequency(hz)
        │         → osc.set_decay(60000 / DEFAULT_DECAY_MS)
        │         → osc.trigger(pluck_pos, pickup_pos, amplitude)
        │             → seeds delay line with triangle at amplitude
        │         → envelope.trigger() → Stage::Sustain (level = 1.0)
        │
        └── NoteOff
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
        │      mix[i] += tmp[i] * velocity_gain * envelope_gain
        │
        │  for idle voices:
        │    if pending note exists → trigger it
        │    else → osc.clear(), voice.active = false
        │
        │  for each frame:
        │    tanh(mix[i] * SATURATION_DRIVE) / SATURATION_DRIVE
        │    clamp to [-1.0, 1.0]
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

This synth replaces the sine oscillator from the previous version with a Karplus-Strong plucked string model. The core idea is a circular delay line seeded with a triangle wave that circulates through a one-pole lowpass filter in a feedback loop. Each pass attenuates high frequencies slightly more than low ones, producing the natural brightness-to-warmth decay of a plucked string.

### Karplus-Strong Model

The delay line length determines pitch: `delay_len = sample_rate / f0 - 0.5`. The `- 0.5` compensates for the half-sample delay introduced by the averaging lowpass filter, which improves tuning accuracy. Fractional delay is handled with linear interpolation between adjacent samples so non-integer delay lengths are exact.

The feedback gain is computed to target a specific T60 decay time regardless of pitch. Without compensation, the averaging filter causes high notes to decay much faster than low ones. The gain formula:

```
G  = 10^(-decay_db_per_sec / (20 * f0))   — target gain per loop
A  = cos(π * f0 / fs)                      — LP filter's own amplitude response
g  = min((G / A) * 0.5, 0.4995)            — compensated feedback gain
```

The hard cap at `0.4995` prevents `g` from reaching or exceeding `0.5` at high frequencies, which would cause net loop gain and a string that never decays, creating pop sounds.

The delay line is seeded with a triangle wave rather than white noise. The peak of the triangle is at `pluck_pos` (0 = bridge end, 0.5 = middle), which shapes the harmonic content: plucking near the bridge emphasises higher harmonics for a bright attack, plucking at the centre gives a rounder, warmer tone.

Output is tapped from `pickup_pos` along the delay line using interpolated reads, modelling microphone or pickup placement on the string.

### Amplitude and Gain Staging

Each voice bakes its amplitude into the delay line seed at trigger time:

```
amplitude = velocity / 127.0 / sqrt(MAX_VOICES)
```

Dividing by `sqrt(MAX_VOICES)` means the voices scale logarithmically. This is applied once at seed time, no dynamic gain correction applied, meaning voices don't change volume suddenly.

A tanh saturator acts as a safety limiter on the final mix output:

```
out = tanh(mix * SATURATION_DRIVE) / SATURATION_DRIVE
```

With `SATURATION_DRIVE >= 1.0` the output is always within `(-1, 1)` before
the clamp. Lower values of drive give cleaner output; higher values add soft
saturation character when voices push the signal hard.

### ADSR as a Release Gate

KS has its own physical amplitude envelope built into the decay model, so the ADSR is simplified to four stages: Attack (linear ramp from 0.0 to 1.0), Sustain (holds at 1.0 after trigger), Release (linear fade to zero on note-off), and Kill (fast fade used for voice stealing). A inperceptively short attack is required to get rid of a click when the note turns on. Decay stage is removed entirely.

### Voice Stealing

Stealing an active voice has the same behavior as the previous program, entering a kill stage, then triggering the pending note when the voice reaches silence.

However, as the delay line of the string model doesn't match the ADSR behavior, when a voice goes idle naturally (after release), `osc.clear()` zeroes the delay line before deactivating, preventing stale string content from leaking into the next note triggered on that voice.

### DC Blocker

As waveguides are prone to accumulating DC, a simple highpass filter is implemented at the end of `process()` in `osc.cpp`. 

### Latency

As real-time audio is on a strict deadline, making sure the program has ample time to process audio is important. How long it takes the kernel to schedule a thread is downtime from the OS that could ruin a deadline.

A tool called `cyclictest` can be used to test how long this scheduling takes:

```bash
sudo apt install rt-tests
sudo cyclictest -l100000 -m -p80 -i1000
```

This will measure the kernel thread scheduling delay 100,000 times with a 1ms delay with a `SCHED_FIFO` priority of 80.

For my Pi 5, this test resulted in a min and average of 2µs, and a max of 12µs. This is extremely low, as a buffer size of 64 at 44.1kHz sample rate is around 1450µs of budget per period. This amount of scheduling latency is negligigle for my buffer size.

If the scheduler were a problem, one can install the PREEMPT-RT kernel patch. This is a modification of linux kernels to make kernel tasks more often interruptable, giving more consistent access to the CPU to high priority, real-time tasks like audio programs. [This PDF](https://runtimerec.com/wp-content/uploads/2024/10/real-time-performance-in-linux-harnessing-preempt-rt-for-embedded-systems_67219ae1.pdf) provides a detailed run down of what PREEMPT-RT is, why its needed, and how to use it. [This GitHub repo](https://github.com/pbosetti/Raspberry5-RT) walks through the process of installing and compiling a PREEMPT-RT patch on a Raspberry Pi.

For my case, I will not install ther PREEMPT-RT patch, and simply make sure the program runs with a `SCHED_FIFO` with a priority of 80.

```cpp
#include <pthread.h>

void AudioEngine::start() {
    running.store(true);
    thread = std::thread(&AudioEngine::audio_loop, this);

    // elevate above normal scheduler after thread is running
    sched_param sp { .sched_priority = 80 };
    int err = pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &sp);
    if (err != 0)
        std::cerr << "AudioEngine: could not set realtime priority (missing cap_sys_nice?)\n";
}
```

The binary must be given permissions to set its priority. This command can be run to do so:

```bash
sudo setcap cap_sys_nice+ep ./bin/05_pluck_synth
```

However, I have put this command in the `CMakeLists.txt` so it runs automatically after every build:

```cmake
add_custom_command(TARGET 05_pluck_synth POST_BUILD
    COMMAND sudo setcap cap_sys_nice+ep $<TARGET_FILE:05_pluck_synth>
    COMMENT "Granting realtime scheduling capability"
)
```

To make sure the program is running in a priority status, run this command while the program is running:

```bash
ps -eLo pid,tid,cls,rtprio,comm | grep pluck
```

If the program shows as `TS`, it is in `timeshare` mode, meaning it is sharing in equal status, and the `SCHED_FIFO` isn't working.

If it shows as `FF`, it is in `FIFO` mode, and it has priority status.
