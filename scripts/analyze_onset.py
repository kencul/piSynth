#!/usr/bin/env python3
# Finds the audio onset in a WAV file and computes round-trip latency.
# Called by benchmark_latency.sh; not intended for direct use.
#
# Usage: python3 analyze_onset.py <wav_file> <sample_rate> <pre_trigger_seconds>

import math
import struct
import sys
import wave

PERIOD_SIZE = 64
BUFFER_PERIODS = 4


def rms(samples: list[float]) -> float:
    return math.sqrt(sum(s * s for s in samples) / len(samples)) if samples else 0.0


def find_onset(samples: list[float], noise_floor: float, search_from: int) -> int | None:
    # Threshold = noise floor + 20 dB, never below -60 dBFS
    threshold = max(noise_floor * 10.0, 0.001)

    # Scan in 16-sample steps with a 64-sample RMS window to reject single-sample spikes
    step, window = 16, 64
    for i in range(search_from, len(samples) - window, step):
        if rms(samples[i : i + window]) > threshold:
            for j in range(max(search_from, i - step), i + window):
                if abs(samples[j]) > threshold:
                    return j
    return None


def main() -> None:
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <wav_file> <sample_rate> <pre_trigger_seconds>")
        sys.exit(1)

    wav_path = sys.argv[1]
    sample_rate = int(sys.argv[2])
    pre_trigger_s = float(sys.argv[3])

    with wave.open(wav_path, "r") as f:
        n_frames = f.getnframes()
        n_channels = f.getnchannels()
        sampwidth = f.getsampwidth()
        raw = f.readframes(n_frames)

    if sampwidth != 2:
        print(f"Error: expected 16-bit audio, got {sampwidth * 8}-bit")
        sys.exit(1)

    all_samples = struct.unpack(f"<{n_frames * n_channels}h", raw)
    samples = [all_samples[i * n_channels] / 32768.0 for i in range(n_frames)]

    # Measure noise floor from 0.5–0.7s into the recording (after ALSA init, before note fires)
    nf_start = int(0.5 * sample_rate)
    nf_end = int(0.7 * sample_rate)
    noise_floor = rms(samples[nf_start:nf_end])
    noise_db = 20.0 * math.log10(noise_floor) if noise_floor > 1e-9 else -120.0
    print(f"Noise floor : {noise_db:.1f} dBFS")

    # Search for onset starting 50ms before the expected note fire time
    search_from = max(0, int((pre_trigger_s - 0.05) * sample_rate))
    onset = find_onset(samples, noise_floor, search_from)

    if onset is None:
        print("Error: no onset detected.")
        print("  - Check the cable: DAC headphone out → interface line in")
        print("  - Check the interface input gain")
        print("  - Verify the synth is outputting to the USB-C DAC, not the interface")
        sys.exit(1)

    onset_s = onset / sample_rate
    latency_ms = (onset_s - pre_trigger_s) * 1000.0

    print(f"Onset       : sample {onset} ({onset_s:.4f}s into recording)")
    print(f"Pre-trigger : {pre_trigger_s:.4f}s")
    print()
    print(f"Measured latency     : {latency_ms:.1f} ms")
    print(f"  (±10-15ms from ALSA capture startup; run 5x and average)")

    buffer_size = PERIOD_SIZE * BUFFER_PERIODS
    theo_ms = buffer_size / sample_rate * 1000.0
    print()
    print(f"Theoretical floor    : {theo_ms:.2f} ms  ({buffer_size} samples @ {sample_rate} Hz)")


if __name__ == "__main__":
    main()
