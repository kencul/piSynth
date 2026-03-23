#include "master_bus.hpp"
#include "../config.hpp"
#include <algorithm>
#include <cmath>

MasterBus::MasterBus(SynthParams &params) : params(params) {}

void MasterBus::init(int period_size) {
	chorus.init(period_size,
	            params.value(SynthParams::ParamId::ChorusMix),
	            params.value(SynthParams::ParamId::ChorusRate),
	            params.value(SynthParams::ParamId::ChorusDepth));
}

void MasterBus::process(std::span<float> mix_l, std::span<float> mix_r) {
	float mix   = params.value(SynthParams::ParamId::ChorusMix);
	float rate  = params.value(SynthParams::ParamId::ChorusRate);
	float depth = params.value(SynthParams::ParamId::ChorusDepth);
	chorus.process(mix_l, mix_r, rate, depth, mix);

	float gain = params.value(SynthParams::ParamId::MasterGain);

	for (int i = 0; i < static_cast<int>(mix_l.size()); ++i) {
		// soft clip via tanh, then restore level — acts as a bus limiter
		mix_l[i] = std::tanh(mix_l[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;
		mix_r[i] = std::tanh(mix_r[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;

		mix_l[i] *= gain;
		mix_r[i] *= gain;
	}
}