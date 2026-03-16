#include "osc.hpp"
#include <cstring>

Pluck::Pluck(double sample_rate) : sample_rate(sample_rate) {}

void Pluck::set_frequency(double hz) {
	frequency = hz;
	delay_len = static_cast<float>(sample_rate / hz - 0.5f);
}

void Pluck::set_decay(float decay_db_per_sec) {
	float f0      = static_cast<float>(frequency);
	float fs      = static_cast<float>(sample_rate);
	float G       = std::pow(10.0f, -decay_db_per_sec / (20.0f * f0));
	float A       = std::cos(static_cast<float>(M_PI) * f0 / fs);
	feedback_gain = std::min((G / A) * 0.5f, 0.4995f);
}

float Pluck::next_noise() {
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return (static_cast<float>(rng_state) / 2147483648.0f) - 1.0f;
}

void Pluck::trigger(float pluck_pos, float pickup_pos, float amplitude) {
	pickup_frac = pickup_pos;

	int seed_len = static_cast<int>(std::ceil(delay_len));
	int peak     = std::max(1, std::min(static_cast<int>(pluck_pos * seed_len), seed_len - 1));

	// triangle init scaled by amplitude
	for (int i = 0; i < peak; ++i)
		delay_line[i] = amplitude * static_cast<float>(i) / static_cast<float>(peak);
	for (int i = peak; i < seed_len; ++i)
		delay_line[i] =
		    amplitude * static_cast<float>(seed_len - i) / static_cast<float>(seed_len - peak);

	for (int i = seed_len; i < MAX_DELAY; ++i) delay_line[i] = 0.0f;

	write_pos = seed_len;
	prev      = 0.0f;
}

void Pluck::clear() {
	delay_line.fill(0.0f);
	prev      = 0.0f;
	write_pos = 0;
}

void Pluck::process(float *buf, int frames) {
	for (int i = 0; i < frames; ++i) {
		float pickup_read = static_cast<float>(write_pos) - pickup_frac * delay_len;
		if (pickup_read < 0.0f) pickup_read += static_cast<float>(MAX_DELAY);
		int p0       = static_cast<int>(pickup_read) & (MAX_DELAY - 1);
		int p1       = (p0 + 1) & (MAX_DELAY - 1);
		float pfrac  = pickup_read - std::floor(pickup_read);
		float output = delay_line[p0] + pfrac * (delay_line[p1] - delay_line[p0]);

		float read_pos = static_cast<float>(write_pos) - delay_len;
		if (read_pos < 0.0f) read_pos += static_cast<float>(MAX_DELAY);
		int idx0        = static_cast<int>(read_pos) & (MAX_DELAY - 1);
		int idx1        = (idx0 + 1) & (MAX_DELAY - 1);
		float frac      = read_pos - std::floor(read_pos);
		float from_line = delay_line[idx0] + frac * (delay_line[idx1] - delay_line[idx0]);

		delay_line[write_pos] = (from_line + prev) * feedback_gain;
		prev                  = from_line;
		write_pos             = (write_pos + 1) & (MAX_DELAY - 1);

		buf[i] = output;
	}
}