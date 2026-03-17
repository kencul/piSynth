#include "voice.hpp"
#include <cmath>

void Voice::trigger(int midi_note, double hz, int velocity) {
	note   = midi_note;
	active = true;

    float velocity_gain = std::pow(velocity / 127.0f, 2.0f);

	float amplitude = velocity_gain / std::sqrt(static_cast<float>(Config::MAX_VOICES));

	osc.set_frequency(hz);
	osc.set_decay(60000.0f / Config::DEFAULT_DECAY_MS);
	osc.trigger(Config::PLUCK_POS, Config::PICKUP_POS, amplitude);
	envelope.trigger();
	pending.valid = false;
}

void Voice::steal(int midi_note, double hz, int velocity) {
	pending = {midi_note, hz, velocity, true};
	envelope.kill();
}

void Voice::release() { envelope.release(); }