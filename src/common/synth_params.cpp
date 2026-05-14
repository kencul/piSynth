#include "synth_params.hpp"
#include "../web/msg_parser.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

SynthParams::SynthParams() {
	static_assert(std::atomic<float>::is_always_lock_free,
	              "float atomics must be lock-free on this platform");

	const passwd *pw = getpwuid(getuid());
	presets_dir      = fs::path(pw ? pw->pw_dir : "/tmp") / ".local" / "share" / "pi-synth" / "presets";
	std::cout << "SynthParams: presets_dir = " << presets_dir << "\n";

	// descriptors define the full contract for each param
	descs[static_cast<int>(ParamId::MasterGain)] = {
	    0.0f, 1.0f, 1.0f, ParamScale::Power, "Master Gain", ""};
	descs[static_cast<int>(ParamId::DecayTime)] = {
	    Config::MIN_DECAY_MS, Config::MAX_DECAY_MS, 6000.0f, ParamScale::Log, "Decay", "ms"};
	descs[static_cast<int>(ParamId::PluckPos)] = {
	    0.0f, 0.5f, 0.2f, ParamScale::Linear, "Pluck Pos", ""};
	descs[static_cast<int>(ParamId::PickupPos)] = {
	    Config::MIN_PICKUP_POS, 0.5f, 0.2f, ParamScale::Linear, "Pickup Pos", ""};
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
	descs[static_cast<int>(ParamId::ReverbCutoff)] = {Config::REVERB_MIN_CUTOFF_HZ,
	                                                  Config::REVERB_MAX_CUTOFF_HZ,
	                                                  5000.0f,
	                                                  ParamScale::Exponential,
	                                                  "Reverb Cutoff",
	                                                  "Hz"};
	descs[static_cast<int>(ParamId::ReverbMix)]    = {
        0.0f, 1.0f, 0.5f, ParamScale::Linear, "Reverb Mix", ""};
	descs[static_cast<int>(ParamId::DelayTime)] = {
	    1.0f, Config::PING_PONG_MAX_DELAY_MS, 100.0f, ParamScale::Power, "Delay Time", "ms"};
	descs[static_cast<int>(ParamId::DelayFeedback)] = {
	    0.0f, 0.95f, 0.5f, ParamScale::Linear, "Delay Feedback", ""};
	descs[static_cast<int>(ParamId::DelayMix)] = {
	    0.0f, 1.0f, 0.25f, ParamScale::Linear, "Delay Mix", ""};

	// initialize param values to defaults
	for (int i = 0; i < COUNT; ++i) { set_to_default(static_cast<ParamId>(i)); }

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
	    {27, ParamId::ReverbCutoff},
	    {28, ParamId::ReverbMix},
	    {25, ParamId::DelayTime},
	    {29, ParamId::DelayFeedback},
	    {1, ParamId::DelayMix},
	};
}

void SynthParams::handle_cc(int cc, int value) {
	auto it = cc_map.find(cc);
	if (it == cc_map.end()) return;

	params[static_cast<int>(it->second)].store(value / 127.0f);
}

float SynthParams::get_value(ParamId id) const {
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

float SynthParams::get_normalized(ParamId id) const { return params[static_cast<int>(id)].load(); }

SynthParams::ParamDescriptor SynthParams::descriptor(SynthParams::ParamId id) const {
	return descs[static_cast<int>(id)];
}

std::optional<SynthParams::ParamId> SynthParams::cc_to_param(int cc) const {
	auto it = cc_map.find(cc);
	if (it == cc_map.end()) return std::nullopt;
	return it->second;
}

void SynthParams::set_param(ParamId id, float normalized) {
	params[static_cast<int>(id)].store(std::clamp(normalized, 0.0f, 1.0f));
}

void SynthParams::save_preset(const std::string &name) {
	try {
		fs::create_directories(presets_dir);
		save_to_file((presets_dir / (name + ".json")).string());
	} catch (const fs::filesystem_error &e) {
		std::cerr << "SynthParams: save_preset failed: " << e.what() << "\n";
	}
}

void SynthParams::load_preset(const std::string &name) {
	try {
		load_from_file((presets_dir / (name + ".json")).string());
	} catch (const fs::filesystem_error &e) {
		std::cerr << "SynthParams: load_preset failed: " << e.what() << "\n";
	}
}

void SynthParams::save_to_file(const std::string &path) {
	std::ofstream f(path);
	if (!f) return;

	f << "{\n";
	bool first = true;
	for (int i = 0; i < COUNT; ++i) {
		auto id = static_cast<ParamId>(i);
		if (id == ParamId::MasterGain) continue;
		if (!first) f << ",\n";
		f << "  \"" << i << "\": " << get_normalized(id);
		first = false;
	}
	f << "\n}";
	std::cout << "SynthParams: Saved to " << path << "\n";
}

void SynthParams::load_from_file(const std::string &path) {
	std::ifstream f(path);
	if (!f) {
		std::cout << "SynthParams: No file found at " << path << "\n";
		return;
	}

	char c;
	int id;
	float val;
	while (f >> c) {
		if (c == '"') {
			f >> id;
			f.ignore(256, ':');
			f >> val;
			if (id < 0 || id >= COUNT) continue;
			auto param_id = static_cast<ParamId>(id);
			if (param_id == ParamId::MasterGain) continue;
			set_param(param_id, val);
		}
	}
	std::cout << "SynthParams: Loaded from " << path << "\n";
}

void SynthParams::set_to_default(ParamId id) {
	int idx = static_cast<int>(id);
	auto &d = descs[idx];
	float t;

	switch (d.scale) {
		case ParamScale::Exponential:
			t = std::log(d.default_value / d.min) / std::log(d.max / d.min);
			break;
		case ParamScale::Log: {
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
	params[idx].store(std::clamp(t, 0.0f, 1.0f));
}

void SynthParams::reset_to_defaults() {
	for (int i = 0; i < COUNT; ++i) { set_to_default(static_cast<ParamId>(i)); }
	std::cout << "SynthParams: Reset to default values\n";
}

std::vector<std::string> SynthParams::get_preset_list() {
	std::vector<std::string> presets;
	try {
		if (!fs::exists(presets_dir)) return presets;
		for (const auto &entry : fs::directory_iterator(presets_dir)) {
			if (entry.path().extension() == ".json") {
				presets.push_back(entry.path().stem().string());
			}
		}
	} catch (const fs::filesystem_error &e) {
		std::cerr << "SynthParams: get_preset_list failed: " << e.what() << "\n";
	}
	return presets;
}

void SynthParams::delete_preset(const std::string &name) {
	try {
		fs::path p = presets_dir / (name + ".json");
		if (fs::exists(p)) {
			fs::remove(p);
			std::cout << "SynthParams: Deleted " << p << "\n";
		}
	} catch (const fs::filesystem_error &e) {
		std::cerr << "SynthParams: delete_preset failed: " << e.what() << "\n";
	}
}