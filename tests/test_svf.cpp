#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>

#include "config.hpp"
#include "effects/svf.hpp"

using Catch::Approx;

// Drives the SVF with a pure sine at test_freq_hz and returns linear gain
// (RMS_out / RMS_in). Uses a small amplitude (0.1) to stay in the linear
// regime and avoid tanh saturation in the resonance feedback path.
static float measure_gain(float cutoff_hz, float resonance, float test_freq_hz) {
    Config::SAMPLE_RATE = 44100;
    SVF svf;
    svf.set_cutoff(cutoff_hz);
    svf.set_resonance(resonance);

    const float amplitude = 0.1f;
    const float inc = 2.0f * std::numbers::pi_v<float> * test_freq_hz
                      / float(Config::SAMPLE_RATE);
    float phase = 0.0f;

    // discard initial transient — high-Q filters settle slowly
    const int settle = 16384;
    for (int i = 0; i < settle; ++i) {
        svf.process(amplitude * std::sin(phase));
        phase += inc;
    }

    double sum_in = 0.0, sum_out = 0.0;
    const int measure = 16384;
    for (int i = 0; i < measure; ++i) {
        float s   = amplitude * std::sin(phase);
        phase    += inc;
        float out = svf.process(s);
        sum_in   += double(s) * s;
        sum_out  += double(out) * out;
    }

    return float(std::sqrt(sum_out / sum_in));
}

static float to_db(float linear_gain) {
    return 20.0f * std::log10(linear_gain);
}

// resonance=0 → k=2 → Q=0.5. At the pole frequency ω₀, a 2nd-order LP has
// |H(jω₀)| = Q, so Q=0.5 gives exactly -6.02 dB. The -3dB frequency is below
// the nominal cutoff for an underdamped filter.
TEST_CASE("SVF gain at cutoff is Q-dependent (Q=0.5 → -6 dB)", "[svf]") {
    for (float cutoff : {200.f, 500.f, 1000.f, 4000.f}) {
        float gain_db = to_db(measure_gain(cutoff, 0.0f, cutoff));
        INFO("cutoff=" << cutoff << " Hz,  gain at cutoff=" << gain_db << " dB");
        CHECK(gain_db == Approx(-6.02f).margin(0.5f));
    }
}

// Butterworth Q = 1/√2 requires k = √2, so resonance = 1 - 1/√2 ≈ 0.293.
// At this setting the -3 dB point lands exactly at the cutoff frequency.
TEST_CASE("SVF -3 dB point is at cutoff with Butterworth resonance setting", "[svf]") {
    const float butterworth_resonance = 1.0f - 1.0f / std::numbers::sqrt2_v<float>; // ≈ 0.293
    for (float cutoff : {200.f, 500.f, 1000.f, 4000.f}) {
        float gain_db = to_db(measure_gain(cutoff, butterworth_resonance, cutoff));
        INFO("cutoff=" << cutoff << " Hz,  gain at cutoff=" << gain_db << " dB");
        CHECK(gain_db == Approx(-3.01f).margin(0.5f));
    }
}

// Well below cutoff the LP should be essentially flat (< 1 dB down).
TEST_CASE("SVF passband is flat below cutoff", "[svf]") {
    for (float cutoff : {500.f, 1000.f, 4000.f}) {
        float gain_db = to_db(measure_gain(cutoff, 0.0f, cutoff * 0.1f));
        INFO("cutoff=" << cutoff << " Hz,  passband gain=" << gain_db << " dB");
        CHECK(gain_db > -1.0f);
    }
}

// 2nd-order rolloff is -40 dB/decade; one decade above cutoff should be
// at least -36 dB (conservative to allow for filter implementation variance).
TEST_CASE("SVF stopband attenuates above cutoff", "[svf]") {
    for (float cutoff : {200.f, 500.f, 1000.f}) {
        float gain_db = to_db(measure_gain(cutoff, 0.0f, cutoff * 10.0f));
        INFO("cutoff=" << cutoff << " Hz,  stopband gain at 10x=" << gain_db << " dB");
        CHECK(gain_db < -36.0f);
    }
}

// High resonance should push the gain at cutoff above the Butterworth baseline.
TEST_CASE("SVF resonance creates a gain peak at cutoff", "[svf]") {
    const float cutoff = 1000.0f;
    float gain_flat_db = to_db(measure_gain(cutoff, 0.0f, cutoff));
    float gain_res_db  = to_db(measure_gain(cutoff, 0.9f, cutoff));
    INFO("resonance=0: " << gain_flat_db << " dB,  resonance=0.9: " << gain_res_db << " dB");
    CHECK(gain_res_db > 0.0f);          // peak should exceed 0 dB
    CHECK(gain_res_db > gain_flat_db + 6.0f);  // and be meaningfully above the flat case
}
