#include "osc.hpp"
#include <cmath>

Oscillator::Oscillator(double sample_rate) : sample_rate(sample_rate) {}

void Oscillator::set_frequency(double hz) { phase_inc = (2.0 * M_PI * hz) / sample_rate; }

void Oscillator::process(int32_t *buf, int frames, int channels) {
	for (int i = 0; i < frames; ++i) {
		int32_t sample = static_cast<int32_t>(Config::AMPLITUDE * std::sin(phase));

		for (int ch = 0; ch < channels; ++ch) buf[i * channels + ch] = sample;

		phase += phase_inc;
		if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
	}
}