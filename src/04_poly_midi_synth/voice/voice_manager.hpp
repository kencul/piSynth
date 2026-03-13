#pragma once
#include "../adsr/adsr.hpp"
#include "../config.hpp"
#include "note_event.hpp"
#include "voice.hpp"
#include <array>
#include <cmath>

class VoiceManager {
public:
	void handle(const NoteEvent &ev);
	void process(int32_t *buf, int frames, int channels);

private:
	std::array<Voice, Config::MAX_VOICES> voices;

	// returns index of a free voice, or steals the oldest if all active
	int allocate_voice();

	// returns index of the voice playing this note, or -1
	int find_voice(int note);

	static double midi_to_hz(int note);

	// tracks insertion order for oldest-voice stealing
	int voice_age[Config::MAX_VOICES] = {};
	int age_counter                   = 0;

	float current_gain = 1.0f;
};