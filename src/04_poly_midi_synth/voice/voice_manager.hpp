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

	static double midi_to_hz(int note);

	// tracks insertion order for oldest-voice stealing
	std::array<int, Config::MAX_VOICES> voice_age = {};
	int age_counter                               = 0;
};