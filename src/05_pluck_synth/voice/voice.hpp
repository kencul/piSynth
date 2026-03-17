#pragma once
#include "../adsr/adsr.hpp"
#include "../config.hpp"
#include "../osc/osc.hpp"
#include <cmath>

struct Voice {
	Pluck osc {Config::SAMPLE_RATE};
	ADSR envelope {Config::SAMPLE_RATE};
	int note            = -1;
	bool active         = false;

	struct PendingNote {
		int note;
		double hz;
		int velocity;
		bool valid = false;
	} pending;

	void trigger(int midi_note, double hz, int velocity) {
		note          = midi_note;
		active        = true;
		float velocity_gain = velocity / 127.0f;

		// gain baked into seed: velocity * per-voice scale, never changes after this
		float amplitude = velocity_gain / std::sqrt(static_cast<float>(Config::MAX_VOICES));

		osc.set_frequency(hz);
		osc.set_decay(60000.0f / Config::DEFAULT_DECAY_MS);
		osc.trigger(Config::PLUCK_POS, Config::PICKUP_POS, amplitude);
		envelope.trigger();
		pending.valid = false;
	}

	void steal(int midi_note, double hz, int velocity) {
		pending = {midi_note, hz, velocity, true};
		envelope.kill();
	}

	void release() { envelope.release(); }
};