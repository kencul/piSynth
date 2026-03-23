#include "svf.hpp"
#include "../config.hpp"

SVF::SVF() {
	set_cutoff(8000.0f);
	set_resonance(0.0f);
}

void SVF::set_cutoff(float hz) {
	hz = std::clamp(hz, 20.0f, Config::SAMPLE_RATE * 0.49f);
	if (hz == last_cutoff) return;
	last_cutoff = hz;
	g           = std::tan(std::numbers::pi_v<float> * hz / Config::SAMPLE_RATE);
	update_coefficients();
}

void SVF::set_resonance(float resonance) {
	// k maps 0->2.0 (no resonance) to 0->0.01 (self-oscillation)
	k = std::max(2.0f * (1.0f - std::clamp(resonance, 0.0f, 1.0f)), 0.01f);
	if (resonance == last_resonance) return;
	last_resonance = resonance;
	update_coefficients();
}

void SVF::reset() {
	s1 = 0.0f;
	s2 = 0.0f;
}

float SVF::process(float input) {
	float v3 = input - s2;
	float v1 = a1 * s1 + a2 * v3;
	float v2 = s2 + a2 * s1 + a3 * v3;

	// tanh on resonance state only — warms feedback path without coloring output
	s1 = fast_tanh(2.0f * v1 - s1);
	s2 = 2.0f * v2 - s2;

	return v2; // LP output
}

void SVF::update_coefficients() {
	// resolves the ZDF algebraic loop: den = 1 + g*k + g^2
	float a1_den = 1.0f / (1.0f + g * (g + k));
	a1           = a1_den;
	a2           = g * a1;
	a3           = g * a2;
}

float SVF::fast_tanh(float x) const {
	// Pade approximant, accurate within ~0.5% for |x| < 3
	if (x < -3.0f) return -1.0f;
	if (x > 3.0f) return 1.0f;
	float x2 = x * x;
	return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}