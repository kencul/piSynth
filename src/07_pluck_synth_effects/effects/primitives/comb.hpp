#pragma once
#include "delay_line.hpp"
#include "one_pole.hpp"

class Comb {
public:
	void init(int delay_ms, float cutoff_hz);
	float process(float input);
	void set_gain(float gain) { this->gain = gain; }
	void set_cutoff(float cutoff_hz);

private:
	DelayLine delay;
	float delay_samples {0.0f};
	float gain {0.5f};
	OnePole filter;
};