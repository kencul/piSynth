#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>

#include "config.hpp"
#include "effects/primitives/lfo.hpp"

using Catch::Approx;

// ── Range ──

// All shapes must stay within [−1, 1] for every sample across a full cycle.
TEST_CASE("LFO all shapes output within [-1, 1]", "[lfo]") {
	Config::SAMPLE_RATE = 44100;
	const int N         = 44100; // 1 Hz rate → one full cycle

	for (auto shape : {LFO::Shape::Sine, LFO::Shape::Triangle, LFO::Shape::Square,
	                   LFO::Shape::SawUp, LFO::Shape::SawDown}) {
		LFO lfo;
		lfo.set_shape(shape);
		lfo.set_rate(1.0f);
		lfo.reset();

		float lo = 1.0f, hi = -1.0f;
		for (int i = 0; i < N; ++i) {
			float v = lfo.process();
			lo      = std::min(lo, v);
			hi      = std::max(hi, v);
		}
		INFO("shape=" << static_cast<int>(shape) << "  min=" << lo << "  max=" << hi);
		CHECK(lo >= -1.0f);
		CHECK(hi <= 1.0f);
	}
}

// ── Shape values at known phase positions ──

// set_phase_offset(p) writes p directly into the phase accumulator, so the next process() call
// reads the shape value at that exact phase before advancing.

TEST_CASE("LFO Sine: sin(2π·phase) at quarter-cycle landmarks", "[lfo]") {
	Config::SAMPLE_RATE = 44100;
	LFO lfo;
	lfo.set_shape(LFO::Shape::Sine);
	lfo.set_rate(1.0f);

	lfo.set_phase_offset(0.0f);
	CHECK(lfo.process() == Approx(0.0f).margin(1e-6f));  // sin(0) = 0

	lfo.set_phase_offset(0.25f);
	CHECK(lfo.process() == Approx(1.0f).margin(1e-6f));  // sin(π/2) = 1

	lfo.set_phase_offset(0.5f);
	CHECK(lfo.process() == Approx(0.0f).margin(1e-6f));  // sin(π) ≈ 0

	lfo.set_phase_offset(0.75f);
	CHECK(lfo.process() == Approx(-1.0f).margin(1e-6f)); // sin(3π/2) = −1
}

// Formula: 1 − 4·|phase − 0.5|  → trough at 0, zero-crossing at 0.25 and 0.75, peak at 0.5
TEST_CASE("LFO Triangle: trough at phase 0, peak at 0.5", "[lfo]") {
	Config::SAMPLE_RATE = 44100;
	LFO lfo;
	lfo.set_shape(LFO::Shape::Triangle);
	lfo.set_rate(1.0f);

	lfo.set_phase_offset(0.0f);
	CHECK(lfo.process() == Approx(-1.0f).margin(1e-6f)); // 1 − 4·0.5 = −1

	lfo.set_phase_offset(0.25f);
	CHECK(lfo.process() == Approx(0.0f).margin(1e-6f));  // 1 − 4·0.25 = 0

	lfo.set_phase_offset(0.5f);
	CHECK(lfo.process() == Approx(1.0f).margin(1e-6f));  // 1 − 4·0 = 1

	lfo.set_phase_offset(0.75f);
	CHECK(lfo.process() == Approx(0.0f).margin(1e-6f));  // 1 − 4·0.25 = 0
}

// Square returns the literal constants 1.0f and −1.0f with no arithmetic: exact equality.
TEST_CASE("LFO Square: +1 in first half, −1 in second half", "[lfo]") {
	Config::SAMPLE_RATE = 44100;
	LFO lfo;
	lfo.set_shape(LFO::Shape::Square);
	lfo.set_rate(1.0f);

	lfo.set_phase_offset(0.0f);
	CHECK(lfo.process() == 1.0f);

	lfo.set_phase_offset(0.25f);
	CHECK(lfo.process() == 1.0f);

	lfo.set_phase_offset(0.5f);
	CHECK(lfo.process() == -1.0f);

	lfo.set_phase_offset(0.75f);
	CHECK(lfo.process() == -1.0f);
}

// Formula: 2·phase − 1  → −1 at phase 0, 0 at phase 0.5
TEST_CASE("LFO SawUp: rises from -1 at phase 0 through 0 at phase 0.5", "[lfo]") {
	Config::SAMPLE_RATE = 44100;
	LFO lfo;
	lfo.set_shape(LFO::Shape::SawUp);
	lfo.set_rate(1.0f);

	lfo.set_phase_offset(0.0f);
	CHECK(lfo.process() == Approx(-1.0f).margin(1e-6f));

	lfo.set_phase_offset(0.5f);
	CHECK(lfo.process() == Approx(0.0f).margin(1e-6f));
}

// Formula: 1 − 2·phase  → +1 at phase 0, 0 at phase 0.5
TEST_CASE("LFO SawDown: falls from +1 at phase 0 through 0 at phase 0.5", "[lfo]") {
	Config::SAMPLE_RATE = 44100;
	LFO lfo;
	lfo.set_shape(LFO::Shape::SawDown);
	lfo.set_rate(1.0f);

	lfo.set_phase_offset(0.0f);
	CHECK(lfo.process() == Approx(1.0f).margin(1e-6f));

	lfo.set_phase_offset(0.5f);
	CHECK(lfo.process() == Approx(0.0f).margin(1e-6f));
}

// ── Rate Accuracy ──

// SawUp produces a large discontinuity (~2.0) at the phase wrap while normal per-sample steps are
// tiny (2 · phase_inc). This makes the wrap easy to detect and gives an unambiguous period count.
// Float accumulation over a full cycle can shift the wrap point by at most ±1 sample.
TEST_CASE("LFO rate produces correct cycle period in samples", "[lfo]") {
	Config::SAMPLE_RATE = 44100;

	for (float rate_hz : {1.0f, 2.0f, 5.0f, 10.0f}) {
		LFO lfo;
		lfo.set_shape(LFO::Shape::SawUp);
		lfo.set_rate(rate_hz);
		lfo.reset();

		const int expected = static_cast<int>(std::round(float(Config::SAMPLE_RATE) / rate_hz));

		float prev    = lfo.process();
		int measured  = -1;
		for (int i = 1; i <= expected * 2; ++i) {
			float curr = lfo.process();
			if (curr - prev < -1.5f) { // wrap: jumps from ~+1 to ~−1
				measured = i;
				break;
			}
			prev = curr;
		}

		REQUIRE(measured != -1);
		INFO("rate=" << rate_hz << " Hz,  expected=" << expected << " samples,  measured=" << measured);
		CHECK(std::abs(measured - expected) <= 1);
	}
}

// ── Reset ──

TEST_CASE("LFO reset() returns output to phase-0 value", "[lfo]") {
	Config::SAMPLE_RATE = 44100;
	LFO lfo;
	lfo.set_shape(LFO::Shape::Sine);
	lfo.set_rate(1.0f);
	lfo.reset();

	const float first = lfo.process();
	for (int i = 0; i < 100; ++i) lfo.process();

	lfo.reset();
	CHECK(lfo.process() == Approx(first).margin(1e-6f));
}
