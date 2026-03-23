#include "one_pole.hpp"
#include "../../config.hpp"
#include <algorithm>
#include <cmath>

void OnePole::set_cutoff(float hz) {
	hz          = std::clamp(hz, 20.0f, Config::SAMPLE_RATE * 0.49f);
	float omega = 2.0f * std::numbers::pi_v<float> * hz / Config::SAMPLE_RATE;
	coeff       = std::exp(-omega);
}

float OnePole::process(float input) {
	state = (1.0f - coeff) * input + coeff * state;
	return state;
}