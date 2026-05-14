#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <vector>

#include "config.hpp"
#include "osc/osc.hpp"

using Catch::Approx;

// Measures the fundamental frequency of a triggered Pluck via normalized
// autocorrelation with parabolic interpolation for sub-sample accuracy.
// Searching only within ±20% of the expected period avoids mistaking a harmonic
// partial for the fundamental.
static float measure_pluck_frequency(float target_hz) {
	Config::SAMPLE_RATE = 44100;

	const int SETTLE  = 4096;
	const int MEASURE = 32768;

	Pluck pluck;
	pluck.set_frequency(target_hz);
	pluck.set_decay(5.0f);
	pluck.trigger(0.5f, 0.1f, 1.0f);

	std::vector<float> buf(SETTLE);
	pluck.process(buf);

	std::vector<float> sig(MEASURE);
	pluck.process(sig);

	const float expected_period = float(Config::SAMPLE_RATE) / target_hz;
	const int min_lag           = std::max(2, int(expected_period * 0.8f));
	const int max_lag           = std::min(int(expected_period * 1.2f) + 1, MEASURE / 4);

	// Unbiased ACF: divide by number of terms at each lag
	std::vector<float> acf(max_lag + 2, 0.0f);
	for (int lag = min_lag; lag <= max_lag + 1; ++lag) {
		double sum  = 0.0;
		const int n = MEASURE - lag;
		for (int i = 0; i < n; ++i) sum += double(sig[i]) * sig[i + lag];
		acf[lag] = float(sum / n);
	}

	int best_lag = min_lag;
	for (int lag = min_lag + 1; lag <= max_lag; ++lag)
		if (acf[lag] > acf[best_lag]) best_lag = lag;

	// Parabolic interpolation: refines the peak to sub-sample precision,
	// which matters most at high frequencies where the period is short (e.g.
	// A5 at 880 Hz has a period of only ~50 samples).
	float refined_lag = float(best_lag);
	if (best_lag > min_lag && best_lag < max_lag) {
		const float ym1 = acf[best_lag - 1], y0 = acf[best_lag], yp1 = acf[best_lag + 1];
		const float denom = ym1 - 2.0f * y0 + yp1;
		if (std::abs(denom) > 1e-10f) refined_lag += 0.5f * (ym1 - yp1) / denom;
	}

	return float(Config::SAMPLE_RATE) / refined_lag;
}

static float hz_to_cents(float measured_hz, float target_hz) {
	return 1200.0f * std::log2(measured_hz / target_hz);
}

TEST_CASE("Pluck frequency accuracy is within ±2 cents across MIDI range", "[pluck][tuning]") {
	struct Note {
		const char *name;
		float hz;
	};
	constexpr Note notes[] = {
	    {"E2", 82.41f},
	    {"A3", 220.0f},
	    {"A4", 440.0f},
	    {"E5", 659.26f},
	    {"A5", 880.0f},
	};

	for (auto &[name, hz] : notes) {
		const float measured = measure_pluck_frequency(hz);
		const float cents    = hz_to_cents(measured, hz);
		INFO(name << " (" << hz << " Hz):  measured=" << measured << " Hz,  error=" << cents
		          << " cents");
		CHECK(std::abs(cents) < 2.0f);
	}
}

// Computes the RMS of a slice of buf[start .. start+len).
static float window_rms(const std::vector<float> &buf, int start, int len) {
	double sum = 0.0;
	for (int i = start; i < start + len; ++i) sum += double(buf[i]) * buf[i];
	return float(std::sqrt(sum / len));
}

// Triggers a Pluck at freq_hz with requested_decay_db_per_sec, then measures
// the actual decay rate from the RMS slope between two windows separated in time.
static float measure_decay_db_per_sec(float freq_hz, float requested_decay) {
	Config::SAMPLE_RATE = 44100;

	// Skip the initial transient (first few periods of triangle smoothing)
	const int SKIP   = 4096;
	const int WINDOW = 4096;  // ~93 ms; spans many periods at all test frequencies
	const int GAP    = 22050; // ~0.5 s between window starts: large enough for clear slope

	const int total = SKIP + GAP + WINDOW;
	std::vector<float> buf(total);

	Pluck pluck;
	pluck.set_frequency(freq_hz);
	pluck.set_decay(requested_decay);
	pluck.trigger(0.5f, 0.1f, 1.0f);
	pluck.process(buf);

	const float rms1 = window_rms(buf, SKIP, WINDOW);
	const float rms2 = window_rms(buf, SKIP + GAP, WINDOW);

	// Center-of-window timestamps
	const float t1 = float(SKIP + WINDOW / 2) / float(Config::SAMPLE_RATE);
	const float t2 = float(SKIP + GAP + WINDOW / 2) / float(Config::SAMPLE_RATE);

	const float db1 = 20.0f * std::log10(rms1);
	const float db2 = 20.0f * std::log10(rms2);

	return (db2 - db1) / (t2 - t1); // negative: signal is decaying
}

// ── DC Blocker ──

// The DC blocker is y[n] = x[n] − x[n-1] + 0.9999·y[n-1] with τ = 10000 samples. The triangle
// initialization produces an early mean of ~23% of amplitude; after 2.2τ (22050 samples) the
// residual is below 2%: a greater than 10x reduction.
TEST_CASE("Pluck DC blocker reduces initial DC offset by at least 10x after settling",
          "[pluck][dc]") {
	Config::SAMPLE_RATE = 44100;

	const int EARLY_N = 1024;
	const int SETTLE  = 22050; // 0.5 s = 2.2 time constants
	const int LATE_N  = 4096;

	Pluck pluck;
	pluck.set_frequency(220.0f);
	pluck.set_decay(5.0f);
	pluck.trigger(0.5f, 0.1f, 1.0f);

	std::vector<float> buf(SETTLE + LATE_N);
	pluck.process(buf);

	double early_mean = 0.0;
	for (int i = 0; i < EARLY_N; ++i) early_mean += buf[i];
	early_mean /= EARLY_N;

	double late_mean = 0.0;
	for (int i = SETTLE; i < SETTLE + LATE_N; ++i) late_mean += buf[i];
	late_mean /= LATE_N;

	INFO("early mean=" << early_mean << ",  late mean=" << late_mean
	                   << ",  ratio=" << std::abs(late_mean) / std::abs(early_mean));
	CHECK(std::abs(float(late_mean)) < std::abs(float(early_mean)) * 0.1f);
}

// ── Decay Rate ──

// Test with combinations that keep feedback_gain below the 0.4995 cap. The cap engages for slow
// decays at high frequencies; A3 (220 Hz) stays clear of it across all three decay rates tested
// here.
//
// Tolerance is ±20% because the RMS includes harmonics that decay at different rates than the
// fundamental: slow decays (10 dB/sec) measure slightly fast (harmonics persist and fade unevenly
// across windows), while fast decays (30 dB/sec) measure slightly slow (window 2 is more purely
// fundamental than window 1, making the slope appear shallower than the true fundamental rate).
TEST_CASE("Pluck decay rate matches requested dB/sec", "[pluck][decay]") {
	struct Case {
		float freq_hz;
		float decay_db_per_sec;
	};
	constexpr Case cases[] = {
	    {220.0f, 10.0f},
	    {220.0f, 20.0f},
	    {220.0f, 30.0f},
	    {440.0f, 20.0f},
	};

	for (auto &[freq, decay] : cases) {
		const float measured_rate = measure_decay_db_per_sec(freq, decay);
		INFO("freq=" << freq << " Hz,  requested=" << decay
		             << " dB/sec,  measured=" << -measured_rate << " dB/sec");
		CHECK(std::abs(measured_rate) == Approx(decay).margin(decay * 0.20f));
	}
}
