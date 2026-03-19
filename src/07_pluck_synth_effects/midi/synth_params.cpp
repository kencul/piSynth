#include "synth_params.hpp"
#include <algorithm>
#include <cmath>

SynthParams::SynthParams() {
	static_assert(std::atomic<float>::is_always_lock_free,
	              "float atomics must be lock-free on this platform");

	// descriptors define the full contract for each param
	descs[static_cast<int>(ParamId::MasterGain)] = {
	    0.0f, 1.0f, 1.0f, CCScale::Exp, "Master Gain", ""};
	descs[static_cast<int>(ParamId::DecayTime)] = {
	    Config::MIN_DECAY_MS, Config::MAX_DECAY_MS, 6000.0f, CCScale::Log, "Decay", "ms"};
	descs[static_cast<int>(ParamId::PluckPos)] = {
	    0.0f, 1.0f, 0.2f, CCScale::Linear, "Pluck Pos", ""};
	descs[static_cast<int>(ParamId::PickupPos)] = {
	    Config::MIN_PICKUP_POS, 1.0f, 0.1f, CCScale::Linear, "Pickup Pos", ""};
	descs[static_cast<int>(ParamId::AttackTime)] = {
	    0.1f, Config::MAX_ATTACK_TIME, 0.1f, CCScale::Log, "Attack", "ms"};
	descs[static_cast<int>(ParamId::ReleaseTime)] = {
	    1.0f, Config::MAX_RELEASE_TIME, 100.0f, CCScale::Log, "Release", "ms"};

	// initialize param values to defaults
	for (int i = 0; i < COUNT; ++i) {
		auto &d = descs[i];
		float t = (d.default_value - d.min) / (d.max - d.min);
		params[i].store(std::clamp(t, 0.0f, 1.0f));
	}

	cc_map = {
	    {14, ParamId::MasterGain},
	    {15, ParamId::DecayTime},
	    {16, ParamId::PluckPos},
	    {17, ParamId::PickupPos},
	    {18, ParamId::AttackTime},
	    {19, ParamId::ReleaseTime},
	};
}

void SynthParams::handle_cc(int cc, int value) {
	auto it = cc_map.find(cc);
	if (it == cc_map.end()) return;

	int idx = static_cast<int>(it->second);
	float t = apply_scale(value / 127.0f, descs[idx].scale);
	params[idx].store(t);
}

float SynthParams::value(ParamId id) const {
	int idx = static_cast<int>(id);
	float t = params[idx].load();
	auto &d = descs[idx];
	return d.min + t * (d.max - d.min);
}

float SynthParams::get(ParamId id) const { return params[static_cast<int>(id)].load(); }

SynthParams::ParamDescriptor SynthParams::descriptor(SynthParams::ParamId id) const {
	return descs[static_cast<int>(id)];
}

float SynthParams::apply_scale(float t, CCScale scale) {
	// t is guaranteed 0-1 from handle_cc
	switch (scale) {
		case CCScale::Linear: return t;

		case CCScale::Log:
			// log1p avoids log(0); maps 0->0, 1->1 with compressed low end
			return std::log1p(t * 9.0f) / std::log(10.0f);

		case CCScale::Exp:
			// quadratic curve, maps 0->0, 1->1 with expanded low end
			return t * t;
	}
	return t;
}

std::optional<SynthParams::ParamId> SynthParams::cc_to_param(int cc) const {
	auto it = cc_map.find(cc);
	if (it == cc_map.end()) return std::nullopt;
	return it->second;
}