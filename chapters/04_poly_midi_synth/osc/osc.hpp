#pragma once
#include "../config.hpp"
#include <cstdint>

class Oscillator {
public:
	explicit Oscillator(double sample_rate);

	void set_frequency(double hz);
	void process(float *buf, int frames);

private:
	double sample_rate;
	double phase     = 0.0;
	double phase_inc = 0.0;
};