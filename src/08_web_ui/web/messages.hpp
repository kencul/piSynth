#pragma once
#include "../common/synth_params.hpp"
#include "msg_builder.hpp"
#include <string>

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