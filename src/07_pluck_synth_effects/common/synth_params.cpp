#include "synth_params.hpp"
#include <algorithm>
#include <cmath>

SynthParams::SynthParams() {
	static_assert(std::atomic<float>::is_always_lock_free,
	              "float atomics must be lock-free on this platform");

	// descriptors define the full contract for each param
	descs[static_cast<int>(ParamId::MasterGain)] = {
	    0.0f, 1.0f, 1.0f, ParamScale::Power, "Master Gain", ""};
	descs[static_cast<int>(ParamId::DecayTime)] = {
	    Config::MIN_DECAY_MS, Config::MAX_DECAY_MS, 6000.0f, ParamScale::Log, "Decay", "ms"};
	descs[static_cast<int>(ParamId::PluckPos)] = {
	    0.0f, 1.0f, 0.2f, ParamScale::Linear, "Pluck Pos", ""};
	descs[static_cast<int>(ParamId::PickupPos)] = {
	    Config::MIN_PICKUP_POS, 1.0f, 0.2f, ParamScale::Linear, "Pickup Pos", ""};
	descs[static_cast<int>(ParamId::AttackTime)] = {
	    0.1f, Config::MAX_ATTACK_TIME, 0.1f, ParamScale::Log, "Attack", "ms"};
	descs[static_cast<int>(ParamId::ReleaseTime)] = {
	    1.0f, Config::MAX_RELEASE_TIME, 100.0f, ParamScale::Log, "Release", "ms"};
	descs[static_cast<int>(ParamId::FilterCutoff)] = {
	    20.0f, 18000.0f, 8000.0f, ParamScale::Exponential, "Filter Cutoff", "Hz"};
	descs[static_cast<int>(ParamId::FilterResonance)] = {
	    0.0f, 1.0f, 0.5f, ParamScale::Linear, "Filter Resonance", ""};
	descs[static_cast<int>(ParamId::ChorusRate)]  = {Config::CHORUS_MIN_RATE_HZ,
	                                                 Config::CHORUS_MAX_RATE_HZ,
	                                                 1.0f,
	                                                 ParamScale::Log,
	                                                 "Chorus Rate",
	                                                 "Hz"};
	descs[static_cast<int>(ParamId::ChorusDepth)] = {
	    0.0f, Config::CHORUS_MAX_DEPTH_MULT, 1.0f, ParamScale::Linear, "Chorus Depth", ""};
	descs[static_cast<int>(ParamId::ChorusMix)] = {
	    0.0f, 1.0f, 0.0f, ParamScale::Linear, "Chorus Mix", ""};
	descs[static_cast<int>(ParamId::ReverbRoomSize)] = {
	    0.0f, 1.0f, 0.9f, ParamScale::Linear, "Reverb Room Size", ""};
	descs[static_cast<int>(ParamId::ReverbDamping)] = {
	    0.0f, 1.0f, 0.5f, ParamScale::Linear, "Reverb Damping", ""};
	descs[static_cast<int>(ParamId::ReverbMix)] = {
	    0.0f, 1.0f, 0.3f, ParamScale::Linear, "Reverb Mix", ""};
		
	// initialize param values to defaults
	for (int i = 0; i < COUNT; ++i) {
		auto &d = descs[i];
		float t;
		switch (d.scale) {
			case ParamScale::Exponential:
				t = std::log(d.default_value / d.min) / std::log(d.max / d.min);
				break;
			case ParamScale::Log: {
				// invert: log1p(t*9)/log(10) = normalized → solve for t
				float norm = (d.default_value - d.min) / (d.max - d.min);
				t          = (std::pow(10.0f, norm) - 1.0f) / 9.0f;
				break;
			}
			case ParamScale::Power: {
				float norm = (d.default_value - d.min) / (d.max - d.min);
				t          = std::sqrt(norm);
				break;
			}
			default: t = (d.default_value - d.min) / (d.max - d.min); break;
		}
		params[i].store(std::clamp(t, 0.0f, 1.0f));
	}

	cc_map = {
	    {14, ParamId::MasterGain},
	    {15, ParamId::DecayTime},
	    {16, ParamId::PluckPos},
	    {17, ParamId::PickupPos},
	    {18, ParamId::AttackTime},
	    {19, ParamId::ReleaseTime},
	    {20, ParamId::FilterCutoff},
	    {21, ParamId::FilterResonance},
	    {22, ParamId::ChorusRate},
	    {23, ParamId::ChorusDepth},
	    {24, ParamId::ChorusMix},
		{26, ParamId::ReverbRoomSize},
		{27, ParamId::ReverbDamping},
		{28, ParamId::ReverbMix},
	};
}

void SynthParams::handle_cc(int cc, int value) {
	auto it = cc_map.find(cc);
	if (it == cc_map.end()) return;

	params[static_cast<int>(it->second)].store(value / 127.0f);
}

float SynthParams::value(ParamId id) const {
	int idx = static_cast<int>(id);
	float t = params[idx].load();
	auto &d = descs[idx];

	switch (d.scale) {
		case ParamScale::Linear: return d.min + t * (d.max - d.min);

		case ParamScale::Log:
			// compresses low end, good for time params
			t = std::log1p(t * 9.0f) / std::log(10.0f);
			return d.min + t * (d.max - d.min);

		case ParamScale::Power:
			// expands low end, good for gain
			t = t * t;
			return d.min + t * (d.max - d.min);

		case ParamScale::Exponential:
			// equal intervals per octave, good for frequency
			return d.min * std::pow(d.max / d.min, t);
	}
	return d.min;
}

float SynthParams::get(ParamId id) const { return params[static_cast<int>(id)].load(); }

SynthParams::ParamDescriptor SynthParams::descriptor(SynthParams::ParamId id) const {
	return descs[static_cast<int>(id)];
}

std::optional<SynthParams::ParamId> SynthParams::cc_to_param(int cc) const {
	auto it = cc_map.find(cc);
	if (it == cc_map.end()) return std::nullopt;
	return it->second;
}