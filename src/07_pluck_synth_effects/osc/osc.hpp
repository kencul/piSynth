#pragma once
#include "../config.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

class Pluck {
public:
	explicit Pluck(double sample_rate);

	void set_frequency(double hz);
	void set_decay(float decay_db_per_sec);

	// amplitude: pre-scaled gain baked into the noise seed
	void trigger(float pluck_pos, float pickup_pos, float amplitude);

	// zeros the delay line
	void clear();

	void process(std::span<float> buf);

private:
	float interpolate_delay_line(float read_idx_float);
	static constexpr int MAX_DELAY = 8192;

	std::array<float, MAX_DELAY> delay_line = {};
	int write_pos;

	float delay_len;
	// Half the total delay length, representing one-way physical string length.
	float half_delay_len;

	// Normalized position (0.0=nut, 1.0=bridge) where output is "picked up".
	float pickup_pos_norm;

	float prev          = 0.0f;
	float feedback_gain = 0.4995f;

	double sample_rate;
	double frequency = 440.0;

	// DC block filter state
	float dc_x = 0.0f; // previous input
	float dc_y = 0.0f; // previous output
};