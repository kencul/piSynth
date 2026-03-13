#include "osc.hpp"
#include <cmath>

Oscillator::Oscillator(double sample_rate) : sample_rate(sample_rate) {}

void Oscillator::set_frequency(double hz) { phase_inc = (2.0 * M_PI * hz) / sample_rate; }

void Oscillator::process(float *buf, int frames) {
	for (int i = 0; i < frames; ++i) {
		buf[i] = static_cast<float>(std::sin(phase));
		phase += phase_inc;
		if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
	}
}