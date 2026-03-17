#pragma once
#include "../adsr/adsr.hpp"
#include "../config.hpp"
#include "../osc/osc.hpp"

class Voice {
public:
	void trigger(int midi_note, double hz, int velocity);
	void steal(int midi_note, double hz, int velocity);
	void release();

	Pluck osc {Config::SAMPLE_RATE};
	ADSR envelope {Config::SAMPLE_RATE};
	int note    = -1;
	bool active = false;

	struct PendingNote {
		int note;
		double hz;
		int velocity;
		bool valid = false;
	} pending;

	float pan_left  = 1.0f;
	float pan_right = 1.0f;
};