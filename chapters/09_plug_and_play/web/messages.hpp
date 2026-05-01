#pragma once
#include "../common/synth_params.hpp"
#include "../common/waveguide_snapshot.hpp"
#include "../config.hpp"
#include "msg_builder.hpp"
#include <string>

struct ConfigMsg {
	int sample_rate;
	int spectrum_bins;

	std::string serialize() const {
		return JsonMsg("config")
		    .field("sample_rate", sample_rate)
		    .field("spectrum_bins", spectrum_bins)
		    .str();
	}
};

struct MeterMsg {
	float rms_l, rms_r, peak_l, peak_r;

	std::string serialize() const {
		return JsonMsg("meter")
		    .field("rms_l", rms_l)
		    .field("rms_r", rms_r)
		    .field("peak_l", peak_l)
		    .field("peak_r", peak_r)
		    .str();
	}
};

struct WaveguideMsg {
	WaveguideSnapshot snap;

	std::string serialize() const {
		return JsonMsg("waveguide")
		    .field("active", static_cast<int>(snap.active))
		    .field("fret_pos", snap.fret_pos)
		    .field("pickup_pos", snap.pickup_pos)
		    .array("displacement", snap.displacement.data(), WaveguideSnapshot::POINTS)
		    .str();
	}
};

struct SpectrumMsg {
	std::array<float, Config::FFT_OUT_BINS> bins;

	std::string serialize() const {
		std::string s = R"({"type":"spectrum","bins":[)";
		char tmp[16];
		for (int i = 0; i < Config::FFT_OUT_BINS; ++i) {
			if (i > 0) s += ',';
			auto [ptr, _] =
			    std::to_chars(tmp, tmp + sizeof(tmp), bins[i], std::chars_format::fixed, 1);
			s.append(tmp, ptr);
		}
		s += "]}";
		return s;
	}
};

struct ParamMsg {
	SynthParams::ParamId id;
	float normalized;
	float value;
	const char *name;
	const char *unit;

	std::string serialize() const {
		return JsonMsg("param")
		    .field("id", static_cast<int>(id))
		    .field("normalized", normalized)
		    .field("value", value)
		    .field("name", name)
		    .field("unit", unit)
		    .str();
	}
};

struct MIDIDeviceMsg {
	std::string midi_devices; // Comma-separated list

	std::string serialize() const {
		return JsonMsg("midiDevice").field("midi", midi_devices).str();
	}
};

struct AudioDeviceMsg {
	std::string audio_device;

	std::string serialize() const {
		return JsonMsg("audioDevice").field("audio", audio_device).str();
	}
};

struct PresetListMsg {
	std::vector<std::string> presets;

	std::string serialize() const { return JsonMsg("preset_list").array("presets", presets).str(); }
};