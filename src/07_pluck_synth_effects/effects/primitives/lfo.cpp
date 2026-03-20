#include "lfo.hpp"
#include "../../config.hpp"
#include <algorithm>
#include <cmath>

LFO::LFO() { set_rate(1.0f); }

void LFO::set_rate(float hz) { phase_inc = hz / Config::SAMPLE_RATE; }

void LFO::set_shape(Shape shape) { this->shape = shape; }

void LFO::set_phase_offset(float offset) { phase = std::fmod(offset, 1.0f); }

void LFO::reset() { phase = 0.0f; }

float LFO::process() {
	float value = 0.0f;
	switch (shape) {
		case Shape::Sine: value = std::sin(2.0f * std::numbers::pi * phase); break;
		case Shape::Triangle: value = 1.0f - 4.0f * std::abs(phase - 0.5f); break;
		case Shape::Square: value = phase < 0.5f ? 1.0f : -1.0f; break;
		case Shape::SawUp: value = 2.0f * phase - 1.0f; break;
		case Shape::SawDown: value = 1.0f - 2.0f * phase; break;
	}

	phase += phase_inc;
	if (phase >= 1.0f) phase -= 1.0f;

	return value;
}