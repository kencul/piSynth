#include "lfo.hpp"
#include "../../config.hpp"
#include <algorithm>
#include <cmath>
#include <numbers> // pi_v

LFO::LFO() { set_rate(1.0f); }

void LFO::set_rate(float hz) { phase_inc = hz / double(Config::SAMPLE_RATE); }

void LFO::set_shape(Shape shape) { this->shape = shape; }

void LFO::set_phase_offset(float offset) { phase = std::fmod(double(offset), 1.0); }

void LFO::reset() { phase = 0.0; }

float LFO::process() {
	float value = 0.0f;
	switch (shape) {
		case Shape::Sine: value = float(std::sin(2.0 * std::numbers::pi_v<double> * phase)); break;
		case Shape::Triangle: value = float(1.0 - 4.0 * std::abs(phase - 0.5)); break;
		case Shape::Square: value = phase < 0.5 ? 1.0f : -1.0f; break;
		case Shape::SawUp: value = float(2.0 * phase - 1.0); break;
		case Shape::SawDown: value = float(1.0 - 2.0 * phase); break;
	}

	phase += phase_inc;
	if (phase >= 1.0) phase -= 1.0;

	return value;
}