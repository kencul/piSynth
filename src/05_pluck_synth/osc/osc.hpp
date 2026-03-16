#pragma once
#include "../config.hpp"
#include <array>
#include <cmath>
#include <cstdint>

class Pluck {
public:
	explicit Pluck(double sample_rate);

	void set_frequency(double hz);
	void set_decay(float decay_db_per_sec);

	// amplitude: pre-scaled gain baked into the noise seed
	void trigger(float pluck_pos, float pickup_pos, float amplitude);

	// zeros the delay line
	void clear();

	void process(float *buf, int frames);

private:
	static constexpr int MAX_DELAY = 8192;

	std::array<float, MAX_DELAY> delay_line = {};
	int write_pos                           = 0;
	float delay_len                         = 100.0f;
	float prev                              = 0.0f;
	float feedback_gain                     = 0.4995f;
	float pickup_frac                       = 0.1f;
	double sample_rate;
	double frequency = 440.0;

	float dc_x = 0.0f; // previous input
	float dc_y = 0.0f; // previous output
};