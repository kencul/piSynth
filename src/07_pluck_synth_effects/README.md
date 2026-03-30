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
 
This version adds effects onto the synth.

### Restructuring of Processing

First, the `voice` class has its own process method. It handles `osc.process`, `envelope.process` and the panning logic out of `voiceManger`.

Then, the `mix_l` and `mix_r` bufferes are moved to `audio.h`. `AudioEngine::audio_loop` handles passing the buffers to `voiceManger` to be filled, then applies `tanh` saturation, clamping, int16 conversion and master gain. This way, `voiceManger` is a coordinator that takes buffers, and asks voices to fill them, handling states of the voices only.

These changes will make implementing master effects and voice effects easier.

### Span

For restructuring purposes, I replaced most references to vectors or arrays with `span`. `Span` holds a pointer to the block of memory, as well as its length, but owns neither. This means that it combines passing `float *buf, int frames` into one object. Furthermore, it enables the use of iterators, such as `begin()` and `end()`, while disallowing memory allocation like `push_back()` or `pop()`. It can also be constructed with implicit conversions from `vector<float>`, `array<float, size>`, and `float*buf[size]`, meaning it is compatible with modern C++ and C callbacks.

### Param Handling

I also restructured the params so that it is the responsibility of `VoiceManager` to send all param data to the relavent objects. `Voice` no longer knows the existance of `SynthParams`.

### Zero Delay SVF

I implemented the same zero delay state variable filter that I made in my [STM32 Polyphonic Synth](https://github.com/kencul/STM32F4-AudioSynthesis/tree/main). It is light to compute, accurate, and stable at high resonance, so it works for this synth as well.

Each voice of the synth has a separate instance of the filter, as it is a stateful filter. 

One significant difference from the STM32 implementation is that I added keytracking to the filter cutoff. in `Voice::process()`:

```cpp
// scale cutoff to note pitch: doubles per octave above C4, halves below
float tracking = std::pow(2.0f, (note - 60) / 12.0f);
```

This means the cutoff the user sets is what is applied to C4. When a note is in a separate octave, the cutoff is scaled. This means that low and high notes get the same timbral effect from the filter.

### Multi MIDI Input

As I ran out of CC knobs on my MIDI controller, I made a quick and scuffed MIDI CC controller with a Teensy 4.0 and one of my dual channel analog mux boards. This adds 8 more pots, with more that can be easily added in the future.

To be able to use this, I converted the `MIDIReader::open` function to take a `initializer_list`, a read only view of brace-enclosed lists of values. In simple terms, `config.hpp` takes a list of MIDI device names in braces and tries to connect all of them to the synth.

### Master Bus

All global audio effeccts are put into a master bus, that runs after `VoiceManager::process()` in the audio loop. The audio loop runs `MasterBus::process()`, which then runs all the effects in one place. To begin, the `tanh` saturation and the master gain are put here.

The `AudioEngine` only handles clamping for emergencies and int16 conversion.

### Restructuring Config Value Fetching

One aspect of programming I struggle with is structuring value fetching in a consistent way.

For instance, in this project, there were a couple ways that the sample rate of the program was fetched by different classes.

The two main methods were passing it by constructor, and fetching from the `Config` class.

Some classes had the sample rate as a float parameter in the constructor. The parent object would pass the sample rate (however it gets it) to the object. This value gets stored as a member variable `sample_rate`. Some others import the `Config` class itself, then access it with `Config::SAMPLE_RATE`.

Here, I decided that importing in each class that requires a value from it is what I'd like. I deleted the sample rate parameter from all constructors, and have the sample rate accessed with no caching whenever it is needed.

### Effect Primitives

Among the effects that will be made, some primitives are shared among them. As I build effects, I will build these primitives as small classes sorted in the `effects/primitives` directory.

### Chorus Effect

First, I built the chorus, as it is the most siginificant effect standalone, and the other effects will be more pronounced with a chorus.

The primitives I built for this effect is a `delay_line` and a `lfo`.

The `delay_line` is a simple ring buffer with linear interpolation read. It has protections against reading outside its bounds.

The `lfo` is a decently robust implementation of an LFO. It handles multiple shapes:

```cpp
enum class Shape { Sine, Triangle, Square, SawUp, SawDown };
```

With these two primitives, creating a chorus effect is simplified. The chorus implemented here is a stereo chorus, where the right and left channels are modulated by a separate lfo and delay line pair.

The LFO modulates the read position of the delay line, causing changing pitch shifting that oscillates around the original pitch. The right and left LFO's run at different rates, where the left one modulates at 1.2x the rate of the chorus, while the right one modulates at 0.8x. This means the right and left channels have different modulation, diverging and converging constantly, creating stereo width.

Furthermore, the two delay lines read from different base delay positions (`Config::CHORUS_LEFT_BASE_MS` and `Config::CHORUS_RIGHT_BASE_MS`). This ensures that regardless of the LFO modulation, the left and right channels sound different. Even if the LFO's have low rate and are coincidentally at similar values, the left and right channels will sound different because the delay times are different.

The depth of the chorus (the amplitude of the LFO) is tied to the rate of the chorus (frequency of the LFO). This can be seen in `Chorus::process()`:

```cpp
float depth_ms = Config::CHORUS_DEPTH_COUPLING / rate;
```

Coupling the depth with the rate is important as increasing the frequency of the LFO means the values change faster, causing more pitch shifting. To ensure changing the rate doesn't affect the depth, the depth is derived from the rate according to a constant (CHORUS_DEPTH_COUPLING). 

### Parameter Smoothing

When sending CC messages, there is cracking that happens. This is because the parameter isn't being smoothed, causing the values to abruptly jump and cause zipper noise. As a one-pole filter is needed for a comb filter to create a reverb anyways, I will design a one-pole filter that can be used as a primitive for a comb filter as well as a paramter smoother.

The `OnePole` class is a simple one pole low pass filter that accepts time, frequency and coefficient values to set its cutoff. This allows it to be used for multiple applications.

The `SmoothedValue` class then takes this `OnePole` filter and wraps it to be used for smoothing parameter values. It takes a ms time value that represents how long it takes to reach 63% of the target value. This is standard definition in electric engineering and DSP, based on exp(-1).

To apply smoothing to a param value with this class, add a `SmoothedValue` object for a param in the class that uses it, init the smoother with `SmoothedValue::reset()` to set the initial value, use `SmoothedValue::set_target` at the beginning of each audio block, then `SmoothedValue::next()` to get the next value every sample.

This is important for params that directly affect the amplitude of the signal, such as the mix level of the chorus and master gain.

Other values like chorus depth and rate want smoothing but doesn't have to be per sample. It can be smoothed per period instead. By adding a PerBlock granularity option, the `SmoothValue` can be used to smooth once per block, applying the smoothing time to be per block rather than per sample.

However, the chorus still has clicking and zipper noise even when smoothing depth and rate by block. This is because these two values combined derive `depth_ms` in `Chorus`, which is the value that moves the read position of the chorus delay lines. This value can still move greatly per sample. So, instead of smoothing rate and depth, smoothing the derived `depth_ms` value per sample resulted in much cleaner output. I also clamped the minimum rate frequency to 1Hz, as changing the rate under 1Hz drastically changes `depth_ms`, causing zipper noise.

Per sample smoothing is applied to the gain in `MasterBus`, as well as to the filter cutoff and resonance.

Applying smoothing to filter cutoff and resonance was slightly complicated. As the cutoff value is passed to each voice, which processes the entire period, each voice needs to be able to get the smoothed values for each sample. However, all 8 voices use the same smoothed value, so having a `SmoothedValue` object for each voice is wasteful. However, changing the cutoff of the filter requires a call to `tan()`, a costly operation. To avoid this, I changed the cutoff to be consistent across a period, meaning ther are 8 `tan()` calls per period (one per voice), instead of a `tan()` per voice per sample, or 256 calls per period. Testing this change made no noticable difference in filter sweeps, so I kept this change.


### Comb Filter

The comb filter, implemented as the `Comb` class, is a delay line with a feedback loop that goes through the `OnePole` filter multiplied by a gain. The output of the delay line is the resulting output of the filter. This results in a impulse response that slowly decays, and an amplitude response that looks like a comb. This filter approximates the reflections off walls that creates reverb. The delay line is the time it takes for the sound to reflect back, the filter in the feedback is the loss of high frequency as it bounces off walls, and the gain reduction is the loss of power as it reflects. To better simulate a reverb, multiple comb filters can be applied in parallel with different delay times to create numerous amounts of reflections.

### Allpass Filter

The allpass filter, implemented as the `Allpass` class, is a delay line with a feedback and a feed forward loop. the feedback is multiplied by the gain, while the feedforward is multiplied by the inverse of gain. The allpass filter results in theoretically flat amplitude response, meaning it doesn't change the sound at all. It does affect the phase of the sound, however, which smears sharp transients, while letting continous sounds through mostly untouched. Applying an allpass therefore smears sharp sounds, such as echoes caused by comb filters.

### Freeverb Filter

The freeverb filter structure has 8 comb filters in parallel going into 4 allpass filters in series. The 8 comb filters are set to specific delay times that do not share common factors. This means these filters cause intermitten echoes of the input audio. However, these comb filters alone sound sparse and distinct, having a metallic characteristic. To make it sound more natural, the resulting output of the comb filters go through 4 allpass filters in a row. These allpass filters have short delay times that decrease in length. Each allpass filter smears the distinct echoes of the comb filters, with each filter further scattering the sound.

The end result is a lush and dense wash of echos that is percieved as reverb. This implementation can be seen in the `Freeverb` class.

To handle stereo, the strucutre is duplicated per channel, and all delays are increased by 5ms on one side. The incoming stereo audio is mixed down to mono, then fed to both channels to intertwine the channels in the reverb.

The room size is derived as such:

```cpp
float feedback = 0.68f + clamped_room_size * 0.28f
```

This value is then set as the gain value for all comb filters.

Damping affects how quickly high frequencies get filtered out in the reverberation. Setting the filter cutoff of each comb filter achieves this effect.

The room size, damping, and mix level is then exposed as parameters controllable with MIDI CC. These are mostly standard:

Room size and mix level are both linearly scaled from 0 to 1. The big difference is that the mix level is smoothed per sample, while room size is per block. This is because if the mix level isn't smoothed per sample, there is crackling, similar to the behavior with the chorus effect.

Cutoff freq is scaled from `Config::REVERB_MIN_CUTOFF_HZ` to `Config::REVERB_MAX_CUTOFF_HZ`, set in `config.hpp`. The paramter has exponential scaling between these values, and the parameter shows the cutoff as Hz values. This paramter has per block smoothing in the reverb effect itself.

### Ping Pong Delay

With the primitives that I've made so far, making a ping pong delay effect is pretty easy. It consists of 2 delay lines, one for each channel, piping into each other in turn. The feedback is treated with a one-pole LPF.

In `config.hpp`, `PING_PONG_MAX_DELAY_MS` has to be set, as the delay lines need a max delay time set. This is set to 1000ms for now. The cutoff frequency of the lowpass filters in the feedback loop is set here as well in `PING_PONG_FEEDBACK_CUTOFF_HZ` to 6000Hz. This value has merit to being a parameter, but my lack of CC knobs forced me to keep this a constant for now.

The exposed parameters are for this effect is the delay time, feedback amount, and mix level. The delay time goes from 1ms to 1000ms with power scaling, as values around 100ms are more musical in this context. In the context with beat matching, longer delay times are musically viable, so more emphasis would be put on the higher ranges, but can be mostly be left alone in this case.

The feedback amound and mix level are both 0-1 linear scalings. The mix level and delay time are both per sample smoothing. Mix level for the same reason as the reverb and chorus, and delay time to make the dopper effect when changing the delay time during delay tails have less artifacts.

### Restructuring interaction between voice and voice manager

Voice manager's logic relies on accessing members of `voice` objects directly. It checks the `active` member variable to check if its available or not, accessing the `envelope` object in `voice` to check the envelope state and so on.

This creates a implicit relationship between the two classes that becomes messy. The solution is to restructure `voice` to create various interface functions that turn long if statement logic in `VoiceManger` into one function call in `voice`. The function in `voice` then handles all the logic, and therefore handles all the interaction with its member variables and objects.

### Sample Rate bug with Smoothed Value

The sample rate is a negotiated value; the ALSA is asked for a sample rate, then ALSA sets the actual sample rate to as close as the requested value as possible. This negotiated value is saved in a mutable global in `config.hpp`, and all but the `SmoothedValue` object handles it.

`SmoothedValue` calculates the coefficient for smoothing based on the time set by the user at init time. This is before the negotiation with ALSA occurs, meaning the requested value is used. If the negotiated value is different, the `SmoothedValue` would have the incorrect coefficient value. To fix this, the coefficient is calculated when `SmoothedValue::reset` is run, which must always be run in the init function. This ensures the coefficient calculation is done with the negotiated sample rate.