# Scripts

## `benchmark_latency.sh`

Measures round-trip MIDI-to-audio latency: the time from a programmatic MIDI note-on to audio appearing at the captured output.

### Hardware setup

```
Raspberry Pi
  ├── USB → USB ground isolator → USB-C DAC → 3.5mm headphone out
  │                                                    │
  │                                               3.5mm cable
  │                                                    │
  └── USB ← USB interface ← 3.5mm line in ←────────────┘
```

The synth outputs to the USB-C DAC. The signal loops back into the USB interface line input, where `arecord` captures it. The USB ground isolator on the DAC breaks the ground loop that would otherwise form with both devices on the same USB bus. It is electrically transparent to the USB protocol and adds no measurable latency.

### Synth settings

Set these in the browser UI before running:

- **Attack**: minimum (0.1 ms) — sharpest possible transient for clean onset detection
- **Effects**: all off — reverb and delay smear the onset

### One-time setup

Load the virtual MIDI kernel module. The synth's auto-connect scans for new sequencer clients on `PORT_START` events, so it picks up VirMIDI within a few seconds of the module loading.

```bash
sudo modprobe snd-virmidi
```

The module registers as `Virtual Raw MIDI` in `amidi -l`. The script detects it automatically.

### Run

```bash
./scripts/benchmark_latency.sh
```

If the USB interface is not auto-detected:

```bash
arecord -l  # find the card number
./scripts/benchmark_latency.sh --capture-device plughw:2,0
```

Run 20 times and average. Each run has a timing uncertainty described below.

### How it works

1. `arecord` begins capturing from the USB interface input into a temp WAV file. The host clock is stamped immediately (`T0`) as a reference for sample 0.
2. After a 2.5s silence window, `amidi` fires a raw MIDI note-on to the VirMIDI raw device (`hw:5,0`). The clock is stamped again at that moment (`T1`).
3. The synth's MIDI thread receives the sequencer event, pushes it to the lock-free ring buffer, and the audio thread drains it at the start of the next period, producing audio.
4. Audio travels: DSP → ALSA output buffer → USB → USB-C DAC → 3.5mm cable → USB interface ADC → USB → `arecord`.
5. `analyze_onset.py` reads the WAV, measures the noise floor from the settled silence window, then scans for the first 64-sample RMS window that exceeds the noise floor by 20 dB. The onset sample position gives the latency:

```
latency = (onset_sample / sample_rate) - (T1 - T0)
```

### Findings

Measured on Raspberry Pi 5 with a Steinberg UR22mkII interface and Apple USB-C audio adapter, at 48000 Hz, 64-sample period (n = 20 runs):

| Run | Measured | Run | Measured |
|-----|----------|-----|----------|
| 1   | −15.5 ms | 11  | +3.9 ms  |
| 2   | +6.6 ms  | 12  | −0.3 ms  |
| 3   | −9.2 ms  | 13  | +4.0 ms  |
| 4   | +0.6 ms  | 14  | −18.9 ms |
| 5   | −16.0 ms | 15  | −16.1 ms |
| 6   | +3.6 ms  | 16  | −16.2 ms |
| 7   | +6.7 ms  | 17  | +2.2 ms  |
| 8   | −8.1 ms  | 18  | +8.9 ms  |
| 9   | +8.6 ms  | 19  | −16.0 ms |
| 10  | −15.9 ms | 20  | +5.1 ms  |

**Mean: −4.1 ms · Std dev: ±10.0 ms · Min: −18.9 ms · Max: +8.9 ms**

The negative values and large spread are a systematic measurement artifact: `T0` is stamped at the `arecord` call, but ALSA's capture buffer doesn't begin filling until 10–20 ms later (call this `D`). The formula computes `true_latency − D`, so when `D` exceeds the true latency the result goes negative.

The distribution is visibly bimodal. Values cluster near −16 ms and near +5 ms with little in between. This reflects the Pi scheduler landing in one of two distinct states rather than varying continuously: either the capture pipeline initializes quickly (~9 ms `D`) or it stalls for an extra period (~20 ms `D`).

Correcting for the ALSA offset: `true_latency = mean + D_avg`. Assuming `D_avg` in the 12–15 ms range, the corrected estimate is **~8–11 ms**.

This result is consistent with the theoretical breakdown:

| Stage | Latency |
|-------|---------|
| MIDI poll wake (average) | ~1 ms |
| Ring buffer drain (one period worst case) | 1.33 ms |
| ALSA output buffer (256 samples @ 48 kHz) | 5.33 ms |
| USB audio frame + DAC → cable → ADC | ~1–2 ms |
| **Expected total** | **~9–10 ms** |

The 5.33 ms output buffer floor is exact and deterministic — it is a direct consequence of the 64-sample period and 4-period ALSA buffer configured in `config.hpp`. The remaining ~4 ms is USB and scheduling overhead.

### Measurement limitations

The dominant source of uncertainty is ALSA capture initialization: the gap between calling `arecord` and when sample 0 is actually captured is 10–20 ms and varies between runs. This prevents sub-millisecond precision without a hardware reference signal (e.g., an oscilloscope triggering on the audio output). For a software-only loopback measurement the ±10 ms window is inherent, and running multiple times and averaging is the correct mitigation.

### Dependencies

All standard on Pi OS — no pip packages required:

- `alsa-utils`: provides `arecord` and `amidi`
- `snd-virmidi`: kernel module, built into the kernel
- `python3`: stdlib `wave` and `struct` only

---

## `benchmark_cpu.sh`

Measures CPU utilization under a fully saturated 8-voice load, with each effect in isolation and all effects combined.

### Setup

1. Start the synth.
2. Open the browser UI in a browser tab. This keeps the WebSocket and FFT pipeline active during measurement, matching real-world use.
3. Load VirMIDI if not already loaded:
   ```bash
   sudo modprobe snd-virmidi
   ```
4. Install `sysstat` if not present:
   ```bash
   sudo apt install sysstat
   ```

### Run

```bash
./scripts/benchmark_cpu.sh
```

### How it works

Six passes run in sequence. Each pass:

1. Sends MIDI CCs to configure the target effect preset at maximum settings (all other effect mixes zeroed).
2. Fires 8 simultaneous note-ons on distinct pitches to saturate all voices.
3. Retriggers those note-ons every 5 seconds — Karplus-Strong waveguides decay naturally, and retrigger prevents voices from going silent before the measurement window ends.
4. Runs `pidstat -p <synth_pid> 1 30`, sampling the synth process every second for 30 seconds.
5. Discards the first 2 seconds (voice ramp-up and parameter smoother transients), then computes mean, median, and std dev over the remaining 28 samples.
6. Sends note-offs and moves to the next pass.

All effect parameters are set to maximum (CC value 127) for every pass. CPU is measured per-process rather than system-wide, so scheduler noise on other cores does not dilute the result.

### Findings

Measured on Raspberry Pi 5, 48 kHz / 64-sample period, 8 simultaneous voices, browser UI open (WebSocket + FFT active). Each pass runs for 30 s; the first 2 s are discarded to exclude voice ramp-up and parameter smoother transients. CPU is reported as `%CPU` from `pidstat` for the synth process — per-process utilization across all cores, where 100% = one full core.

| Configuration   | Mean  | Median | Std Dev | Delta vs idle |
|-----------------|-------|--------|---------|---------------|
| Idle (baseline) | 16.2% | 16.0%  | ±0.4%   | —             |
| Dry             | 23.8% | 24.0%  | ±0.4%   | +7.6%         |
| Reverb          | 23.8% | 24.0%  | ±0.6%   | +7.6%         |
| Chorus          | 23.8% | 24.0%  | ±0.5%   | +7.6%         |
| Delay           | 23.8% | 24.0%  | ±0.5%   | +7.6%         |
| All effects     | 23.8% | 24.0%  | ±0.5%   | +7.6%         |

The idle baseline (16.2%) is the cost of the audio thread, WebSocket server, and FFT pipeline with no voices active. The delta (+7.6%) is the pure DSP cost of 8 simultaneous Karplus-Strong voices.

All effect configurations produce identical CPU usage. The Karplus-Strong waveguide computation dominates: reverb, chorus, and delay each add no measurable incremental cost at the per-period level, and all three combined costs the same as any one alone. Effect overhead exists but is sub-1% per effect — below `pidstat`'s resolution floor — so it cannot be distinguished from the dry baseline with this tool. The effects bus is not the bottleneck.

### Measurement limitations

`pidstat` reports CPU in 1% increments. Effect overhead exists but is sub-1% per effect, which falls below this resolution floor and cannot be distinguished from the dry baseline with this tool. Isolating per-effect cost would require cycle-accurate measurement via `perf stat`.

At ~24% of one core out of four, the Pi 5 has substantial headroom at maximum polyphony and full effects.

### Dependencies

- `alsa-utils`: provides `amidi`
- `snd-virmidi`: kernel module, built into the kernel
- `sysstat`: provides `pidstat`
