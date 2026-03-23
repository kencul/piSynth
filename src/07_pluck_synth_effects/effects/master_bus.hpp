#pragma once
#include "../common/smoothed_value.hpp"
#include "../common/synth_params.hpp"
#include "chorus.hpp"
#include <span>

class MasterBus {
public:
	explicit MasterBus(SynthParams &params);

	void init();
	void process(std::span<float> mix_l, std::span<float> mix_r);

private:
	SynthParams &params;

	Chorus chorus;

	SmoothedValue gain_smoother {20.0f};
};