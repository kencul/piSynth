#include "master_bus.hpp"
#include "../config.hpp"
#include <algorithm>
#include <cmath>

MasterBus::MasterBus(SynthParams &params) : params(params) {}

void MasterBus::init() {
	chorus.init(params.value(SynthParams::ParamId::ChorusMix),
	            params.value(SynthParams::ParamId::ChorusRate),
	            params.value(SynthParams::ParamId::ChorusDepth));
	freeverb.init(params.value(SynthParams::ParamId::ReverbRoomSize),
	              params.value(SynthParams::ParamId::ReverbDamping),
	              params.value(SynthParams::ParamId::ReverbMix));

	gain_smoother.reset(params.value(SynthParams::ParamId::MasterGain));
}

void MasterBus::process(std::span<float> mix_l, std::span<float> mix_r) {
	float chorus_mix   = params.value(SynthParams::ParamId::ChorusMix);
	float rate  = params.value(SynthParams::ParamId::ChorusRate);
	float depth = params.value(SynthParams::ParamId::ChorusDepth);
	chorus.process(mix_l, mix_r, rate, depth, chorus_mix);

	float room_size = params.value(SynthParams::ParamId::ReverbRoomSize);
	float damping   = params.value(SynthParams::ParamId::ReverbDamping);
	float verb_mix       = params.value(SynthParams::ParamId::ReverbMix);

	freeverb.process(mix_l, mix_r, room_size, damping, verb_mix);

	gain_smoother.set_target(params.value(SynthParams::ParamId::MasterGain));

	for (int i = 0; i < static_cast<int>(mix_l.size()); ++i) {
		float gain = gain_smoother.next();
		// soft clip via tanh, then restore level — acts as a bus limiter
		mix_l[i] = std::tanh(mix_l[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;
		mix_r[i] = std::tanh(mix_r[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;

		mix_l[i] *= gain;
		mix_r[i] *= gain;
	}
}