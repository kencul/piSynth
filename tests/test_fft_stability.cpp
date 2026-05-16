// fft stability / correctness diagnostic
// Run: build/bin/fft_debug
// Feeds known sine waves through the full FftAccumulator + FftProcessor pipeline
// and measures frame-to-frame stability (the "shaking" metric).
//
// Three scenarios:
//   1. Steady 1 kHz sine  – peak bin should be stable across all overlapping frames
//   2. Silence            – all bins should sit at -80 dB (floor)
//   3. Two-tone (1 kHz + 4 kHz) – both peaks should be present and stable
//
// Output is human-readable; pipe through grep/awk or redirect to a CSV for plotting.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numbers>
#include <numeric>
#include <vector>

#include "fft/fft_accumulator.hpp"
#include "fft/fft_processor.hpp"
#include "config.hpp"

// Mirror the accumulator size used by the synth
using Acc = FftAccumulator<Config::FFT_ACC_SIZE>;

// ── helpers ────────────────────────────────────────────────────────────────

static float peak_bin_hz(const SpectrumMsg &msg, float sr)
{
    // The OUT_BINS span log10(60 Hz) … log10(sr/2) uniformly in log space.
    const float log_min = std::log10(60.0f);
    const float log_max = std::log10(sr / 2.0f);
    int peak = 0;
    for (int b = 1; b < Config::FFT_OUT_BINS; ++b)
        if (msg.bins[b] > msg.bins[peak]) peak = b;
    float t = static_cast<float>(peak) / (Config::FFT_OUT_BINS - 1);
    return std::pow(10.0f, log_min + t * (log_max - log_min));
}

// Only considers bins above the display floor — sub-floor sidelobe oscillations
// are invisible on screen and would otherwise dominate the metric.
static constexpr float VISIBLE_FLOOR_DB = -79.f;

static float max_bin_delta(const SpectrumMsg &a, const SpectrumMsg &b)
{
    float d = 0.0f;
    for (int i = 0; i < Config::FFT_OUT_BINS; ++i)
        if (a.bins[i] > VISIBLE_FLOOR_DB || b.bins[i] > VISIBLE_FLOOR_DB)
            d = std::max(d, std::abs(a.bins[i] - b.bins[i]));
    return d;
}

static float mean_bin_delta(const SpectrumMsg &a, const SpectrumMsg &b)
{
    float s = 0.0f;
    for (int i = 0; i < Config::FFT_OUT_BINS; ++i)
        s += std::abs(a.bins[i] - b.bins[i]);
    return s / Config::FFT_OUT_BINS;
}

// Fill `acc` with `n_samples` of a multi-tone sine and return how many
// samples were actually accepted (drops if acc is full).
static int fill_sine(Acc &acc, float freq_hz, float sr, int n_samples,
                     float &phase)
{
    int written = 0;
    const float inc = 2.0f * std::numbers::pi_v<float> * freq_hz / sr;
    for (int i = 0; i < n_samples; ++i) {
        if (!acc.write(std::sin(phase))) break;
        phase += inc;
        ++written;
    }
    return written;
}

static int fill_two_tone(Acc &acc, float f1, float f2, float sr, int n_samples,
                          float &ph1, float &ph2)
{
    int written = 0;
    const float inc1 = 2.0f * std::numbers::pi_v<float> * f1 / sr;
    const float inc2 = 2.0f * std::numbers::pi_v<float> * f2 / sr;
    for (int i = 0; i < n_samples; ++i) {
        float s = 0.5f * (std::sin(ph1) + std::sin(ph2));
        if (!acc.write(s)) break;
        ph1 += inc1;
        ph2 += inc2;
        ++written;
    }
    return written;
}

// ── scenario runner ────────────────────────────────────────────────────────

// Feed `total_samples` into `acc` in chunks of `chunk_sz` (simulates
// the audio period) and collect every SpectrumMsg that the processor emits.
// Returns the collected frames.
static std::vector<SpectrumMsg> run_scenario(
    Acc &acc, FftProcessor &proc,
    std::function<void(Acc &, int)> feed_chunk,
    int total_samples, int chunk_sz)
{
    acc.reset();
    proc.reset();
    std::vector<SpectrumMsg> frames;

    for (int sent = 0; sent < total_samples; sent += chunk_sz) {
        feed_chunk(acc, chunk_sz);
        if (auto m = proc.process(acc)) frames.push_back(*m);
    }
    return frames;
}

// ── main ───────────────────────────────────────────────────────────────────

int main()
{
    const float SR         = 44100.0f;
    const int   FFT_SIZE   = Config::FFT_SIZE;          // 4096
    const int   HOP        = FftProcessor::HOP_SIZE;      // matches processor
    const int   CHUNK      = Config::PERIOD_SIZE;        // 64 – audio period
    // Feed enough samples to get ~20 overlapping frames
    const int   TOTAL      = FFT_SIZE + 20 * HOP;

    Acc acc;
    FftProcessor proc;
    proc.init();

    // ── Scenario 1: steady 1 kHz sine ─────────────────────────────────────
    {
        std::printf("\n=== Scenario 1: Steady 1000 Hz sine ===\n");
        float phase = 0.0f;
        auto feeder = [&](Acc &a, int n) { fill_sine(a, 1000.0f, SR, n, phase); };
        auto frames = run_scenario(acc, proc, feeder, TOTAL, CHUNK);

        std::printf("Frames collected: %zu  (expected ~%d)\n",
                    frames.size(), TOTAL / HOP - 1);

        if (frames.empty()) {
            std::printf("ERROR: no frames produced!\n");
        } else {
            // Peak frequency per frame
            std::printf("\nFrame  PeakHz   MaxDelta  MeanDelta\n");
            for (size_t i = 0; i < frames.size(); ++i) {
                float hz = peak_bin_hz(frames[i], SR);
                float md = (i > 0) ? max_bin_delta(frames[i], frames[i - 1]) : 0.0f;
                float av = (i > 0) ? mean_bin_delta(frames[i], frames[i - 1]) : 0.0f;
                std::printf("%5zu  %7.1f  %8.2f  %9.2f\n", i, hz, md, av);
            }

            // Summary
            float max_delta = 0.0f, max_peak_err = 0.0f;
            for (size_t i = 2; i < frames.size(); ++i) {
                max_delta    = std::max(max_delta, max_bin_delta(frames[i], frames[i - 1]));
                max_peak_err = std::max(max_peak_err,
                                        std::abs(peak_bin_hz(frames[i], SR) - 1000.0f));
            }
            std::printf("\nSummary:\n");
            std::printf("  Worst inter-frame max-bin delta : %.2f dB  (>1 dB = visible shake)\n",
                        max_delta);
            std::printf("  Worst peak-frequency error      : %.1f Hz  (target 1000 Hz)\n",
                        max_peak_err);
        }
    }

    // ── Scenario 2: silence ────────────────────────────────────────────────
    {
        std::printf("\n=== Scenario 2: Silence ===\n");
        auto feeder = [](Acc &a, int n) {
            for (int i = 0; i < n; ++i) a.write(0.0f);
        };
        auto frames = run_scenario(acc, proc, feeder, TOTAL, CHUNK);
        std::printf("Frames collected: %zu\n", frames.size());
        if (!frames.empty()) {
            float max_db = *std::max_element(frames[0].bins.begin(), frames[0].bins.end());
            std::printf("Max bin value in first frame: %.1f dB  (should be -80.0)\n", max_db);
        }
    }

    // ── Scenario 3: two-tone 1 kHz + 4 kHz ───────────────────────────────
    {
        std::printf("\n=== Scenario 3: Two-tone 1000 Hz + 4000 Hz ===\n");
        float ph1 = 0.0f, ph2 = 0.0f;
        auto feeder = [&](Acc &a, int n) {
            fill_two_tone(a, 1000.0f, 4000.0f, SR, n, ph1, ph2);
        };
        auto frames = run_scenario(acc, proc, feeder, TOTAL, CHUNK);
        std::printf("Frames collected: %zu\n", frames.size());

        if (!frames.empty()) {
            // Find the two highest peaks in the first frame
            auto &b = frames[0].bins;
            int p1  = static_cast<int>(std::max_element(b.begin(), b.end()) - b.begin());
            // Blank around p1 to find p2
            auto tmp = b;
            int  blk = Config::FFT_OUT_BINS / 20;
            for (int i = std::max(0, p1 - blk); i < std::min(Config::FFT_OUT_BINS, p1 + blk); ++i)
                tmp[i] = -999.0f;
            int p2 = static_cast<int>(std::max_element(tmp.begin(), tmp.end()) - tmp.begin());

            auto bin_hz = [&](int idx) {
                float t = static_cast<float>(idx) / (Config::FFT_OUT_BINS - 1);
                return std::pow(10.0f, std::log10(60.0f) + t * (std::log10(SR / 2.0f) - std::log10(60.0f)));
            };

            std::printf("Peak 1: bin %d  %.1f Hz  %.1f dB\n", p1, bin_hz(p1), b[p1]);
            std::printf("Peak 2: bin %d  %.1f Hz  %.1f dB\n", p2, bin_hz(p2), b[p2]);

            // Inter-frame stability for two-tone
            float max_delta = 0.0f;
            for (size_t i = 2; i < frames.size(); ++i)
                max_delta = std::max(max_delta, max_bin_delta(frames[i], frames[i - 1]));
            std::printf("Worst inter-frame max-bin delta: %.2f dB\n", max_delta);
        }
    }

    // ── Scenario 4: accumulator timing stress ─────────────────────────────
    // Simulate the real-time case: audio thread writes PERIOD_SIZE samples,
    // the FFT thread reads once every N periods. If N varies, we may peek
    // at different positions → shaking.
    {
        std::printf("\n=== Scenario 4: Timing stress (variable read interval) ===\n");
        std::printf("(Simulates FFT thread sometimes being a period late)\n");
        acc.reset();
        proc.reset();

        const int FRAMES_TO_TEST = 30;
        float     phase          = 0.0f;
        std::vector<SpectrumMsg> frames;
        frames.reserve(FRAMES_TO_TEST);

        // Alternate: sometimes write 1 period, sometimes 2, before each process() call.
        for (int f = 0; f < FRAMES_TO_TEST * 4 && (int)frames.size() < FRAMES_TO_TEST; ++f) {
            int periods = (f % 3 == 0) ? 2 : 1;  // every 3rd tick, write double
            for (int p = 0; p < periods; ++p)
                fill_sine(acc, 1000.0f, SR, CHUNK, phase);
            if (auto m = proc.process(acc)) frames.push_back(*m);
        }

        std::printf("Frames collected: %zu\n", frames.size());
        float max_delta = 0.0f;
        for (size_t i = 2; i < frames.size(); ++i)
            max_delta = std::max(max_delta, max_bin_delta(frames[i], frames[i - 1]));
        std::printf("Worst inter-frame max-bin delta: %.2f dB\n", max_delta);
        // Threshold above the ~61 dB shoulder leakage baseline seen under fixed timing.
        // A higher value here means the variable-interval caller is causing extra shake.
        std::printf(max_delta > 65.0f
            ? "WARN: shoulder delta exceeds fixed-timing baseline — hop may be non-deterministic.\n"
            : "OK: stable under timing variation.\n");
    }

    proc.destroy();
    std::printf("\nDone.\n");
    return 0;
}
