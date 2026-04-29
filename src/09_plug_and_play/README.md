# 08 Web UI
A polyphonic pluck-string waveguide synthesizer with a browser-based control and visualization interface, built on top of the 07 synth engine.

## Usage
Build and run:
```bash
cd build
cmake ..
ninja
./bin/08_web_ui
```

Launching will open the audio device, read MIDI controllers, and start a web server. To find the URL, run `hostname` in the Pi console — then open `http://<hostname>.local:<UI_PORT>` in a browser on the same network. The page is served with no-cache headers so the browser always loads the latest version.

To change the audio device or the MIDI sources, edit `config.hpp`:
```cpp
inline constexpr const char *AUDIO_DEVICE = "hw:A";
inline constexpr std::initializer_list<const char *> MIDI_DEVICES = {
    "KOMPLETE KONTROL",
    "Teensy MIDI"
};
```

Adjust voice parameters, effects settings, and web UI refresh rate in `config.hpp`:
```cpp
inline constexpr int   MAX_VOICES             = 8;
inline constexpr float KILL_MS                = 1.5f;    // ms: voice steal fade time
inline constexpr float FILTER_KEYTRACK        = 0.8f;    // 0.0-2.0 cutoff tracking
inline constexpr int   UI_UPDATES_PER_SECOND  = 45;      // visualization refresh rate
inline constexpr int   UI_PORT                = 9002;    // web server port
inline constexpr int   FFT_SIZE               = 4096;    // FFT window size
inline constexpr int   FFT_OUT_BINS           = 512;     // downsampled bins sent to UI
```

## New in Program 08
Program 08 adds a **Web UI layer** on top of the 07 synth engine. A dedicated web thread runs a `uWebSockets` event loop that serves a browser interface, visualizes audio in real time, and accepts parameter control via sliders — all without touching the audio thread.

### Browser Interface
* **Parameter sliders**: All synth parameters are exposed as sliders in the browser. Changes are broadcast to all connected clients so multiple open tabs stay in sync.
* **RMS Meter**: Stereo RMS and peak meter updated at `UI_UPDATES_PER_SECOND`.
* **Waveguide Display**: The state of the digital waveguide string is sampled and resampled to 128 points for a consistent visualization. Shows the effect of pluck position and pickup position parameters in real time.
* **Spectrum Analyzer**: An EQ-style spectrum using a 4096-sample Blackman-Harris windowed FFT, log-scaled and downsampled to `FFT_OUT_BINS` bins before sending.

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
├── creates MsgDispatcher (routes inbound WebSocket messages by type)
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
├── creates WebServer(params, dispatcher)
│     ├── loads index.html into memory
│     └── owns FftProcessor (Blackman-Harris FFT wrapper)
├── audio.open()  → configures ALSA PCM (S16_LE, 48000Hz)
├── midi.open()   → connects to multiple devices in config.hpp
├── wire callbacks
│     ├── audio.on_meter    → web.broadcast(MeterMsg)
│     ├── audio.on_waveguide → web.broadcast(WaveguideMsg)
│     ├── midi.on_param_change → web.broadcast(ParamMsg)
│     └── dispatcher.on("set_param") → params.set_param() + web.broadcast(ParamMsg)
├── web.set_fft_acc(&audio.get_fft_acc())
├── audio.start() → launches high-priority audio thread
├── midi.start()  → launches MIDI polling thread
└── web.start(Config::UI_PORT) → launches web thread (uWS event loop)
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
                           → on_param_change() callback
                                → loop->defer(ParamMsg) → WEB THREAD
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

5. VISUALIZATION CALLBACKS (every meter_interval blocks)
   a. Accumulate RMS/peak per sample across the period
   b. on_meter()     → loop->defer(MeterMsg)    → WEB THREAD
   c. Pluck::snapshot() → resample to 128 pts
      on_waveguide() → loop->defer(WaveguideMsg) → WEB THREAD
   d. fft_acc.write(mono sample) per sample
      (FFT is read and processed by the web thread on its timer)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
WEB THREAD (WebServer::run, uWS event loop)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
INBOUND (Browser → Synth)
  Browser JS sends JSON over WebSocket
        ↓
  .message handler → MsgDispatcher::dispatch(msg)
        → reads "type" field (expected first key)
        → routes to registered handler, e.g. "set_param"
              → MsgParser::extract_int/float → params.set_param()
              → loop->defer(ParamMsg) → broadcast to all clients

OUTBOUND (Synth → Browser)
  loop->defer() callbacks queued by audio/MIDI threads execute here
        ↓
  broadcast_direct() → serializes struct to JSON → sends to all WS clients

  us_timer fires every (1000 / UI_UPDATES_PER_SECOND) ms
        ↓
  FftProcessor::process(fft_acc) → Blackman-Harris window → real FFT
        → magnitude conversion → log frequency scaling
        → downsample to FFT_OUT_BINS
        → broadcast_direct(SpectrumMsg)

  On new client connect (.open):
        → send_initial_state(ws)
              → ConfigMsg (sample_rate, spectrum_bins)
              → ParamMsg for every parameter (syncs sliders to current state)
```

## Code Breakdown
 
This program further enhances the usability of the synth by making the program more plug and play.

## MIDI Devices

To this point, the program has required setting MIDI device names to connect to the program in the config file. If the devices aren't present on program launch, they won't ever connect, and will exit out if no devices are found. If the devices are disconnected mid program or a user tries to connect it mid way, the program doesn't react. Ideally, all connected MIDI devices that have output are connected automatically, regardless of when it is plugged in.

### Auto Connect All

First, I implemented connecting all MIDI output devices to the synth.

This meant iterating through all MIDI devices that are connected, iterating through each port on the device to see if there are any MIDI output ports, then connecting it to the synth.

This search needs to ignore specific devices such as itself and some system devices such as 'System' and 'MIDI thru'.

Although there are some MIDI output ports (such as clock, system, and duplicate ports) that would be ideal to ignore, the MIDI devices I own do not have these, so I did not implement protections for these as I cannot test them.

This is implemented in `connect_all_inputs()` in `midi.cpp`.

### Hotswap Devices

To handle MIDI devices that are connected mid program, the 'System' MIDI device that was ignored is very useful. It broadcasts when a client or port is created or destroyed.

First the System client is connected to the synth in 'open()':

```cpp
// Subscribe to system announcements
snd_seq_connect_from(seq, in_port, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);
```

The system client will then send specific signals when ports and clients are created and destroyed. The relavent events for this program are `SND_SEQ_EVENT_PORT_START` and `SND_SEQ_EVENT_CLIENT_EXIT`.

When a port is created, it may be a output port, so it must be checked. When a client is removed, that means a device was disconnected.

These messages must be handled in `handle_event`:

```cpp
case SND_SEQ_EVENT_PORT_START:
        // A new port available
        std::cout << "MidiReader: New MIDI port detected. Re-scanning...\n";
        connect_all_inputs();
        break;

case SND_SEQ_EVENT_CLIENT_EXIT:
        // A device was removed.
        // ALSA kills the connection automatically
        std::cout << "MidiReader: Device disconnected.\n";
        break;
```

Nothing needs to be done when a device is disconnected, as ALSA handles the cleanup for the most part.

When a new port is created, however, the scan for each client and port is done again. This means the program will attempt to connect all output ports again.

This is, for the most part, fine. If an already connected port is attemped to be connected, a harmless error will be returned. `connect_all_inputs()` is adjusted to ignore this error, so actually unintended errors get printed:

```cpp
int err = snd_seq_connect_from(seq, in_port, client_id, port_id);

if (err == 0) {
        std::cout << "Auto-connected: " << name << " [" << client_id << ":" << port_id
                        << "]\n";
} else if (err == -EEXIST || err == -EBUSY) {
        // Already connected
} else {
        std::cerr << "Failed to connect to " << name << ": " << snd_strerror(err)
                        << "\n";
}
```

## Audio Devices

Next is handling audio output. Currently, the audio output has the device name, sample rate, and bit depth hard coded. If the specified device is not present when the program is run, it fails to boot.

Ideally instead, the program looks for any USB output, determines the sample rate and bit depth that the device can handle, and use it.

### Auto Device Find

First, with the assumption that all audio output will be done by USB audio devices, I can search for any connected USB audio devices. Instead of a hard coded device name, the program iterates through all connected audio devices and selects the first one that includes `USB` in its device name:

```cpp
std::string AudioEngine::find_usb_device() {
	int card = -1;
	while (snd_card_next(&card) == 0 && card >= 0) {
		char *name;
		snd_card_get_name(card, &name);
		std::string card_name = name;

		free(name);

		// Find cards with USB in the name
		if (card_name.find("USB") != std::string::npos) {
			return "plughw:" + std::to_string(card) + ",0";
		}
	}
	return "default"; // Fallback
}
```

The result of this function is used to select the audio output device.

### plughw

When selecting the audio device, it can be selected as a `plughw`. This is a virtual wrapper that automatically converts audio data automatically into a format the audio device can handle. This means, I can send buffers at float in any sample rate I desire, and ALSA will convert to a format the audio device can handle for me.

It is not ideal to offload all conversions to this wrapper however. Resampling for sample rate introduces CPU overhead and aliasing, and downsampling by the wrapper to int 16 won't have the dithering that I added.

Therefore, the sample rate should be properly negotiated. In `AudioEngine::configure_device()`:

```cpp
// Rate Priority: 48000 then 44100
unsigned int rate = 48000;
if (snd_pcm_hw_params_set_rate(handle, hw_params, rate, 0) < 0) {
        rate = 44100;
        if (snd_pcm_hw_params_set_rate(handle, hw_params, rate, 0) < 0) {
                if (snd_pcm_hw_params_set_rate_near(handle, hw_params, &sample_rate, &dir) < 0) {
                        std::cerr << "AudioEngine: could not set sample rate\n";
                        return false; // Couldn't set sample rate
                }
        }
}
this->sample_rate = rate;
```

It first tries to set the sample rate to 48k, then 44.1k, then whatever is closest to 44.1k. The set sample rate is then saved to the member variable to be shared throughout the program.

For the bit depth, the ideal situation is to use floats. Especially for 24 bit devices, there are devices that need 3 byte packets and some that need 4 byte packets. The 3 byte format is especially difficult to handle, as there is no native C++ solution for it.

Therefore, offloading the conversion from floats to 32 bit and 24 bit relieves a lot of headache.

However, I want to handle the conversion to int16 myself, as I want apply dithering to this case. The lower bit depth makes dithering a necessity, as the noise is too noticable. In `AudioEngine::configure_device()`:

```cpp
// Check for high-bitrate support to decide on float vs dithered int16
if (snd_pcm_hw_params_test_format(handle, hw_params, SND_PCM_FORMAT_S32_LE) == 0
        || snd_pcm_hw_params_test_format(handle, hw_params, SND_PCM_FORMAT_S24_LE) == 0
        || snd_pcm_hw_params_test_format(handle, hw_params, SND_PCM_FORMAT_S24_3LE) == 0) {
        // Hardware is high-res; we will provide floats and let plughw convert
        snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_FLOAT_LE);
        use_floats = true;
} else {
        // Fallback to 16-bit; we will dither manually in the loop
        snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
        use_floats = false;
}
```

Then in `AudioEngine::audio_loop()`, send plain floats or convert to int16 with dithering based on the `use_floats` flag:

```cpp
if (use_floats) {
        float *f_ptr = reinterpret_cast<float *>(buf.data());

        for (size_t i = 0; i < period_size; ++i) {
                f_ptr[i * channels + 0] = std::clamp(mix_l[i], -1.0f, 1.0f);
                f_ptr[i * channels + 1] = std::clamp(mix_r[i], -1.0f, 1.0f);
        }
} else {
        int16_t *s16_ptr = reinterpret_cast<int16_t *>(buf.data());

        for (int i = 0; i < static_cast<int>(period_size); ++i) {
                // Apply TPDF dithering and convert to int16
                // Scale by 1/4 of int16 to get half of LSB with rng with a width of 2 (x2 from RNG,
                // to get 0.5 from 2, divide by 4)
                float dither_scale = 0.25f / static_cast<float>(Config::SAMPLE_SCALE);
                float l_noise = (distribution(generator) + distribution(generator)) * dither_scale;
                float r_noise = (distribution(generator) + distribution(generator)) * dither_scale;
                float l_dithered = mix_l[i] + (l_noise);
                float r_dithered = mix_r[i] + (r_noise);

                buf[i * channels + 0] = static_cast<int16_t>(std::clamp(l_dithered, -1.0f, 1.0f)
                                                                * Config::SAMPLE_SCALE);
                buf[i * channels + 1] = static_cast<int16_t>(std::clamp(r_dithered, -1.0f, 1.0f)
                                                                * Config::SAMPLE_SCALE);

                fft_acc.write((l_dithered + r_dithered) * 0.5f);
        }
}
```

`buf` is a vector of int8, that is reinterpreted to float or int16 based on the flag.

### Refining Finding Audio Devices

There were some issues with the device finding process.

First, searching for USB in the name of the device was hit or miss. For instance, my audio interface that I usually use for my PC doesn't have USB in its name, meaning it isn't used by the program.

The solution to this problem was to get the driver info. This involves getting the snd_ctl for each card, which contains metadata. Then, within the metadata, the driver name can be fetched. If this driver name is "USB-Audio", its a USB audio device.

```cpp
snd_ctl_t *ctl;
std::string card_id = "hw:" + std::to_string(card);

if (snd_ctl_open(&ctl, card_id.c_str(), 0) < 0) continue;

snd_ctl_card_info_t *info;
snd_ctl_card_info_alloca(&info);

if (snd_ctl_card_info(ctl, info) >= 0) {
        std::string driver = snd_ctl_card_info_get_driver(info);

        // Check for USB-Audio driver
        if (driver.find("USB-Audio") != std::string::npos){
```

However, MIDI devices are also classified as USB audio devices. To avoid attempting to connect audio to MIDI devices, the program iterates through the devices in the card to look for PCM playback streams. This way, a speaker/interface can be distinguished from a MIDI controller.

```cpp
// Query the card for PCM playback devices
int dev           = -1;
bool has_playback = false;
while (snd_ctl_pcm_next_device(ctl, &dev) == 0 && dev >= 0) {
        snd_pcm_info_t *pcm_info;
        snd_pcm_info_alloca(&pcm_info);
        snd_pcm_info_set_device(pcm_info, dev);
        snd_pcm_info_set_subdevice(pcm_info, 0);
        snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_PLAYBACK);

        if (snd_ctl_pcm_info(ctl, pcm_info) >= 0) {
                has_playback = true;
                break;
        }
}

if (!has_playback) {
        snd_ctl_close(ctl);
        continue; // Skip MIDI-only "Audio" devices
}
```

This process iterates through all available sound cards until it finds a valid output or it reaches the end of the list.

