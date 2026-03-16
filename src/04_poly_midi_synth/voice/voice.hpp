#pragma once
#include "../adsr/adsr.hpp"
#include "../config.hpp"
#include "../osc/osc.hpp"

struct Voice {
	Oscillator osc {Config::SAMPLE_RATE};
	ADSR envelope {Config::SAMPLE_RATE};
	int note            = -1;
	bool active         = false;
	float velocity_gain = 1.0f;

	struct PendingNote {
		int note;
		double hz;
		int velocity;
		bool valid = false;
	} pending;

	void trigger(int midi_note, double hz, int velocity) {
		note          = midi_note;
		active        = true;
		velocity_gain = velocity / 127.0f;
		osc.set_frequency(hz);
		envelope.trigger();
	}

	void steal(int midi_note, double hz, int velocity) {
		pending = {midi_note, hz, velocity, true};
		envelope.kill();
	}

	void release() { envelope.release(); }
};