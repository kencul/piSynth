#include "voice.hpp"
#include <algorithm>
#include <cmath>

void Voice::trigger(int midi_note, double hz, int velocity) {
	note   = midi_note;
	active = true;

	float pan = std::clamp((midi_note - 64) * (Config::PAN_SPREAD / Config::PAN_SEMITONES),
	                       -Config::PAN_SPREAD,
	                       Config::PAN_SPREAD);
	float p   = (pan + 1.0f) * 0.5f;
	// equal power panning
	pan_left  = std::cos(p * static_cast<float>(M_PI) * 0.5f);
	pan_right = std::sin(p * static_cast<float>(M_PI) * 0.5f);

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