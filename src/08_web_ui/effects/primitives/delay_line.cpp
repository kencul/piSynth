#include "delay_line.hpp"
#include <algorithm>
#include <cassert>

void DelayLine::init(int max_samples) {
	buffer.resize(max_samples, 0.0f);
	write_pos = 0;
}

void DelayLine::write(float sample) {
	buffer[write_pos] = sample;
	write_pos         = (write_pos + 1) % buffer.size();
}

float DelayLine::read(float delay_samples) const {
	assert(delay_samples < static_cast<float>(buffer.size()));

	int delay_int    = static_cast<int>(delay_samples);
	float delay_frac = delay_samples - delay_int;

	int idx0 = (write_pos - delay_int + static_cast<int>(buffer.size())) % buffer.size();
	int idx1 = (write_pos - delay_int - 1 + static_cast<int>(buffer.size())) % buffer.size();

	return buffer[idx0] * (1.0f - delay_frac) + buffer[idx1] * delay_frac;
}

void DelayLine::clear() { std::fill(buffer.begin(), buffer.end(), 0.0f); }