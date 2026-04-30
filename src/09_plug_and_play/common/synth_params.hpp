#pragma once
#include "../config.hpp"
#include <array>
#include <atomic>
#include <fstream>
#include <optional>
#include <sstream>
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
		FilterCutoff,
		FilterResonance,
		ChorusRate,
		ChorusDepth,
		ChorusMix,
		ReverbRoomSize,
		ReverbCutoff,
		ReverbMix,
		DelayTime,
		DelayFeedback,
		DelayMix,
		COUNT
	};

	enum class ParamScale { Linear, Log, Power, Exponential };

	struct ParamDescriptor {
		float min;
		float max;
		float default_value;
		ParamScale scale;
		const char *name;
		const char *unit;
	};

	SynthParams();

	// called by MIDI thread
	void handle_cc(int cc, int value);

	// called by audio thread
	float get_value(ParamId id) const;

	std::optional<ParamId> cc_to_param(int cc) const;

	// called by UI
	float get_normalized(ParamId id) const;
	ParamDescriptor descriptor(ParamId id) const;

	// called by the web thread to apply an inbound slider change
	void set_param(ParamId id, float normalized);

	void save_preset(const std::string &name);
	void load_preset(const std::string &name);

	void reset_to_defaults();

private:
	void save_to_file(const std::string &path);
	void load_from_file(const std::string &path);
	void set_to_default(ParamId id);

	static constexpr int COUNT = static_cast<int>(ParamId::COUNT);

	std::array<std::atomic<float>, COUNT> params = {};
	std::array<ParamDescriptor, COUNT> descs     = {};
	std::unordered_map<int, ParamId> cc_map;
};