#pragma once
#include "../common/smoothed_value.hpp"
#include "../config.hpp"
#include "primitives/delay_line.hpp"
#include "primitives/one_pole.hpp"
#include <span>

class PingPong {
public:
	void init(float initial_delay_ms, float initial_feedback, float initial_mix);
	void process(
	    std::span<float> mix_l, std::span<float> mix_r, float delay_ms, float feedback, float mix);

private:
	float ms_to_samples(float ms) const { return ms * 0.001f * Config::SAMPLE_RATE; }

	DelayLine delay_l, delay_r;
	OnePole filter_l, filter_r;

	SmoothedValue delay_smoother {50.0f};
	SmoothedValue feedback_smoother {20.0f, SmoothedValue::Granularity::PerBlock};
	SmoothedValue mix_smoother {20.0f};
};