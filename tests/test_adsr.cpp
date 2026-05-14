#include <catch2/catch_test_macros.hpp>

#include "adsr/adsr.hpp"
#include "config.hpp"

static constexpr unsigned int SR = 44100;

// Counts process() calls until output >= threshold. Returns -1 if not reached within max_samples.
static int samples_to_reach(ADSR &adsr, float threshold, int max_samples) {
	for (int i = 0; i < max_samples; ++i)
		if (adsr.process() >= threshold) return i + 1;
	return -1;
}

// Counts process() calls until is_idle() is true. Returns -1 if not reached within max_samples.
static int samples_to_idle(ADSR &adsr, int max_samples) {
	for (int i = 0; i < max_samples; ++i) {
		adsr.process();
		if (adsr.is_idle()) return i + 1;
	}
	return -1;
}

// ── Initial State ──

TEST_CASE("ADSR starts idle with zero output", "[adsr]") {
	Config::SAMPLE_RATE = SR;
	ADSR adsr;
	CHECK(adsr.is_idle());
	CHECK(adsr.process() == 0.0f);
}

// ── Stage Transitions ──

TEST_CASE("ADSR transitions through idle → attack → sustain → release → idle", "[adsr]") {
	Config::SAMPLE_RATE = SR;
	ADSR adsr;
	adsr.set_attack(10.0f);
	adsr.set_release(10.0f);

	adsr.trigger();
	CHECK_FALSE(adsr.is_idle());
	CHECK_FALSE(adsr.is_releasing());

	{ int guard = SR / 5; while (guard-- > 0 && adsr.process() < 1.0f) {} }
	CHECK_FALSE(adsr.is_idle());
	CHECK_FALSE(adsr.is_releasing());

	adsr.release();
	CHECK(adsr.is_releasing());

	{ int guard = SR / 5; while (guard-- > 0 && !adsr.is_idle()) adsr.process(); }
	CHECK(adsr.is_idle());
	CHECK(adsr.process() == 0.0f);
}

// ── Attack Timing ──

// rate = 1 / (ms * 0.001 * SR), so the linear ramp hits 1.0 in exactly ms * 0.001 * SR samples.
TEST_CASE("ADSR attack timing is within 2 samples of target", "[adsr]") {
	Config::SAMPLE_RATE = SR;

	constexpr float attack_ms = 100.0f;
	constexpr int expected    = int(attack_ms * 0.001f * SR); // 4410
	constexpr int tolerance   = 2;

	ADSR adsr;
	adsr.set_attack(attack_ms);
	adsr.set_release(1000.0f);
	adsr.trigger();

	const int actual = samples_to_reach(adsr, 1.0f, expected + 20);

	REQUIRE(actual != -1);
	INFO("expected=" << expected << " samples,  actual=" << actual);
	CHECK(std::abs(actual - expected) <= tolerance);
}

// ── Sustain Hold ──

TEST_CASE("ADSR sustain holds at 1.0", "[adsr]") {
	Config::SAMPLE_RATE = SR;
	ADSR adsr;
	adsr.set_attack(1.0f);
	adsr.set_release(1000.0f);
	adsr.trigger();

	{ int guard = SR / 5; while (guard-- > 0 && adsr.process() < 1.0f) {} }

	for (int i = 0; i < 1000; ++i) CHECK(adsr.process() == 1.0f);
}

// ── Release Timing ──

TEST_CASE("ADSR release timing is within 2 samples of target", "[adsr]") {
	Config::SAMPLE_RATE = SR;

	constexpr float release_ms = 200.0f;
	constexpr int expected     = int(release_ms * 0.001f * SR); // 8820
	constexpr int tolerance    = 2;

	ADSR adsr;
	adsr.set_attack(1.0f);
	adsr.set_release(release_ms);
	adsr.trigger();

	{ int guard = SR / 5; while (guard-- > 0 && adsr.process() < 1.0f) {} }
	adsr.release();

	const int actual = samples_to_idle(adsr, expected + 20);

	REQUIRE(actual != -1);
	INFO("expected=" << expected << " samples,  actual=" << actual);
	CHECK(std::abs(actual - expected) <= tolerance);
}

// ── Kill ──

// kill() ramps from the current level to 0 over KILL_MS — the fast voice-stealing path.
TEST_CASE("ADSR kill reaches idle within KILL_MS", "[adsr]") {
	Config::SAMPLE_RATE = SR;

	const int kill_budget = int(Config::KILL_MS * 0.001f * SR) + 2;

	ADSR adsr;
	adsr.set_attack(1.0f);
	adsr.set_release(1000.0f);
	adsr.trigger();

	{ int guard = SR / 5; while (guard-- > 0 && adsr.process() < 1.0f) {} }

	adsr.kill();
	CHECK(adsr.is_killing());

	const int actual = samples_to_idle(adsr, kill_budget + 10);

	REQUIRE(actual != -1);
	INFO("kill budget=" << kill_budget << " samples,  actual=" << actual);
	CHECK(actual <= kill_budget);
}

// ── Reset ──

TEST_CASE("ADSR reset returns to idle with zero output mid-attack", "[adsr]") {
	Config::SAMPLE_RATE = SR;
	ADSR adsr;
	adsr.set_attack(100.0f);
	adsr.set_release(100.0f);
	adsr.trigger();

	for (int i = 0; i < 100; ++i) adsr.process();

	adsr.reset();
	CHECK(adsr.is_idle());
	CHECK(adsr.process() == 0.0f);
}
