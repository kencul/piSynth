#pragma once
#include "delay_line.hpp"

class Allpass {
public:
	void init(int delay_ms);
	float process(float input);
	void set_gain(float gain) { this->gain = gain; }

private:
	DelayLine delay;
	float delay_samples {0.0f};
	float gain {0.5f}; // fixed at 0.5 in Freeverb
};