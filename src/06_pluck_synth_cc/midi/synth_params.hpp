#pragma once
#include "config.hpp"
#include <array>
#include <atomic>
#include <optional>
#include <unordered_map>

class SynthParams {
public:
	enum class ParamId {
		MasterGain,
		DecayTime,
		PluckPos,
		PickupPos,
		AttackTime,
		ReleaseTime,
		COUNT
	};

	enum class CCScale { Linear, Log, Exp };

	struct ParamDescriptor {
		float min;
		float max;
		float default_value;
		CCScale scale;
		const char *name;
		const char *unit;
	};
	SynthParams();

	// called by MIDI thread
	void handle_cc(int cc, int value);

	// called by audio thread
	float value(ParamId id) const;

	std::optional<ParamId> cc_to_param(int cc) const;

	// called by UI
	float get(ParamId id) const;
	ParamDescriptor descriptor(ParamId id) const;

private:
	static float apply_scale(float t, CCScale scale);
	static constexpr int COUNT = static_cast<int>(ParamId::COUNT);

	std::array<std::atomic<float>, COUNT> params = {};
	std::array<ParamDescriptor, COUNT> descs     = {};
	std::unordered_map<int, ParamId> cc_map;
};