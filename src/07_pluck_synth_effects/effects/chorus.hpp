#pragma once
#include "../config.hpp"
#include "primitives/delay_line.hpp"
#include "primitives/lfo.hpp"
#include <span>

class Chorus {
public:
	explicit Chorus();
	void init(int period_size);
	void
	process(std::span<float> mix_l, std::span<float> mix_r, float rate, float depth, float wet);

private:
	float ms_to_samples(float ms) const;

	float sample_rate = Config::SAMPLE_RATE;

	DelayLine delay_l, delay_r;
	LFO lfo_l, lfo_r;
};