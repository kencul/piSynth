#include "master_bus.hpp"
#include "../config.hpp"
#include <algorithm>
#include <cmath>

MasterBus::MasterBus(SynthParams &params) : params(params) {}

void MasterBus::init() {
	chorus.init(params.get_value(SynthParams::ParamId::ChorusMix),
	            params.get_value(SynthParams::ParamId::ChorusRate),
	            params.get_value(SynthParams::ParamId::ChorusDepth));
	ping_pong.init(params.get_value(SynthParams::ParamId::DelayTime),
	               params.get_value(SynthParams::ParamId::DelayFeedback),
	               params.get_value(SynthParams::ParamId::DelayMix));
	freeverb.init(params.get_value(SynthParams::ParamId::ReverbRoomSize),
	              params.get_value(SynthParams::ParamId::ReverbCutoff),
	              params.get_value(SynthParams::ParamId::ReverbMix));

	gain_smoother.reset(params.get_value(SynthParams::ParamId::MasterGain));
}

void MasterBus::process(std::span<float> mix_l, std::span<float> mix_r) {
	float chorus_mix   = params.get_value(SynthParams::ParamId::ChorusMix);
	float chorus_rate  = params.get_value(SynthParams::ParamId::ChorusRate);
	float chorus_depth = params.get_value(SynthParams::ParamId::ChorusDepth);
	chorus.process(mix_l, mix_r, chorus_rate, chorus_depth, chorus_mix);

	float delay_mix      = params.get_value(SynthParams::ParamId::DelayMix);
	float delay_time     = params.get_value(SynthParams::ParamId::DelayTime);
	float delay_feedback = params.get_value(SynthParams::ParamId::DelayFeedback);
	ping_pong.process(mix_l, mix_r, delay_time, delay_feedback, delay_mix);

	float reverb_mix         = params.get_value(SynthParams::ParamId::ReverbMix);
	float reverb_room_size   = params.get_value(SynthParams::ParamId::ReverbRoomSize);
	float reverb_cutoff_freq = params.get_value(SynthParams::ParamId::ReverbCutoff);
	freeverb.process(mix_l, mix_r, reverb_room_size, reverb_cutoff_freq, reverb_mix);

	gain_smoother.set_target(params.get_value(SynthParams::ParamId::MasterGain));

	for (int i = 0; i < static_cast<int>(mix_l.size()); ++i) {
		float gain = gain_smoother.next_sample();
		// soft clip via tanh, then restore level — acts as a bus limiter
		mix_l[i] = std::tanh(mix_l[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;
		mix_r[i] = std::tanh(mix_r[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;

		mix_l[i] *= gain;
		mix_r[i] *= gain;
	}
}