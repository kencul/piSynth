#pragma once
#include "../common/smoothed_value.hpp"
#include "../common/synth_params.hpp"
#include "../common/waveguide_snapshot.hpp"
#include "../config.hpp"
#include "note_event.hpp"
#include "voice.hpp"
#include <array>
#include <cmath>

class VoiceManager {
public:
	explicit VoiceManager(SynthParams &params) : params(params) {};
	void init(int period_size);
	void handle(const NoteEvent &ev);
	void process(std::span<float> mix_l, std::span<float> mix_r);
	WaveguideSnapshot snapshot() const;

private:
	SynthParams &params;

	std::array<Voice, Config::MAX_VOICES> voices;

	// returns index of a free voice, or steals the oldest if all active
	int allocate_voice();

	void trigger_note(Voice &voice, int midi_note, double hz, int velocity);
	static double midi_to_hz(int note);

	// tracks insertion order for oldest-voice stealing
	std::array<int, Config::MAX_VOICES> voice_age = {};
	uint32_t age_counter                          = 0;

	SmoothedValue cutoff_smoother {20.0f, SmoothedValue::Granularity::PerBlock};
	SmoothedValue resonance_smoother {20.0f, SmoothedValue::Granularity::PerBlock};
};