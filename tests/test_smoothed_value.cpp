#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "common/smoothed_value.hpp"
#include "config.hpp"

using Catch::Approx;

// ── Reset ──

// reset() writes value directly into filter state and sets target, so the very first next_sample()
// returns that value exactly — no smoothing applied.
TEST_CASE("SmoothedValue reset() snaps to value immediately", "[smoothed_value]") {
	Config::SAMPLE_RATE = 44100;
	SmoothedValue sv(20.0f);
	sv.reset(0.75f);
	CHECK(sv.next_sample() == 0.75f);
}

// With state = target = 0, the filter output is identically 0.
// Snap threshold fires on the first call and holds there — no asymptotic drift.
TEST_CASE("SmoothedValue reset() to zero stays at zero with no target change", "[smoothed_value]") {
	Config::SAMPLE_RATE = 44100;
	SmoothedValue sv(20.0f);
	sv.reset(0.0f);
	for (int i = 0; i < 100; ++i) CHECK(sv.next_sample() == 0.0f);
}

// ── Time Constant ──

// The one-pole filter: state[n] = (1 − c)·target + c·state[n−1], c = e^(−1/N),
// N = time_ms · 0.001 · SR.  Starting from 0 → 1, after exactly N samples:
// state[N] = 1 − c^N = 1 − e^(−1) ≈ 0.6321.
TEST_CASE("SmoothedValue reaches 1−e⁻¹ ≈ 63% of target after one time constant",
          "[smoothed_value]") {
	Config::SAMPLE_RATE = 44100;

	const float time_ms = 50.0f;
	const int N         = static_cast<int>(time_ms * 0.001f * float(Config::SAMPLE_RATE)); // 2205

	SmoothedValue sv(time_ms);
	sv.reset(0.0f);
	sv.set_target(1.0f);

	float out = 0.0f;
	for (int i = 0; i < N; ++i) out = sv.next_sample();

	const float expected = 1.0f - std::exp(-1.0f); // ≈ 0.6321
	INFO("after " << N << " samples: out=" << out << "  expected≈" << expected);
	CHECK(out == Approx(expected).margin(0.01f));
}

// ── Snap Threshold ──

// Without a snap threshold the one-pole tail is infinite. The snap fires when |state − target| <
// snap_threshold (default 1e−4), which happens after ~9τ. After 10τ the output must equal the
// target exactly, not just approximately.
TEST_CASE("SmoothedValue snaps to target exactly and stops drifting", "[smoothed_value]") {
	Config::SAMPLE_RATE = 44100;

	SmoothedValue sv(20.0f);
	sv.reset(0.0f);
	sv.set_target(1.0f);

	// 10 time constants is sufficient: e^(−10) ≈ 4.5e−5 < snap threshold 1e−4
	const int tau = static_cast<int>(20.0f * 0.001f * float(Config::SAMPLE_RATE));
	const int run = tau * 10;
	float out     = 0.0f;
	for (int i = 0; i < run; ++i) out = sv.next_sample();

	CHECK(out == 1.0f);
}

// ── Retarget ──

// set_target() mid-smoothing redirects the filter toward the new value. After enough time the snap
// fires and output must equal the second target exactly.
TEST_CASE("SmoothedValue re-targeting mid-smooth converges to new target", "[smoothed_value]") {
	Config::SAMPLE_RATE = 44100;

	SmoothedValue sv(20.0f);
	sv.reset(0.0f);
	sv.set_target(1.0f);

	// advance half a time constant, then redirect
	const int tau = static_cast<int>(20.0f * 0.001f * float(Config::SAMPLE_RATE));
	for (int i = 0; i < tau / 2; ++i) sv.next_sample();

	sv.set_target(0.2f);

	float out = 0.0f;
	for (int i = 0; i < tau * 10; ++i) out = sv.next_sample();

	CHECK(out == 0.2f);
}
