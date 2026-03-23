#pragma once
#include "../config.hpp"
#include "../midi/smoothed_value.hpp"
#include "primitives/delay_line.hpp"
#include "primitives/lfo.hpp"
#include <span>

class Chorus {
public:
	explicit Chorus();
	void init(int period_size, float initial_mix, float initial_rate, float initial_depth);
	void
	process(std::span<float> mix_l, std::span<float> mix_r, float rate, float depth, float wet);

private:
	float ms_to_samples(float ms) const;

	float sample_rate = Config::SAMPLE_RATE;

	DelayLine delay_l, delay_r;
	LFO lfo_l, lfo_r;

	SmoothedValue mix_smoother {20.0f};
	SmoothedValue depth_smoother {50.0f};
};