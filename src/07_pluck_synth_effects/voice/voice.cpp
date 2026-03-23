#include "voice.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>

void Voice::init(int period_size) { tmp.resize(period_size); }

void Voice::trigger(int midi_note,
                    double hz,
                    int velocity,
                    float attack,
                    float decay,
                    float pluck_pos,
                    float pickup_pos) {
	envelope.set_attack(attack);
	note   = midi_note;
	active = true;

	float pan = std::clamp((midi_note - 64) * (Config::PAN_SPREAD / Config::PAN_SEMITONES),
	                       -Config::PAN_SPREAD,
	                       Config::PAN_SPREAD);
	float p   = (pan + 1.0f) * 0.5f;
	// equal power panning
	pan_left  = std::cos(p * std::numbers::pi_v<float> * 0.5f);
	pan_right = std::sin(p * std::numbers::pi_v<float> * 0.5f);

	float velocity_gain = std::pow(velocity / 127.0f, 2.0f);

	float amplitude = velocity_gain / std::sqrt(static_cast<float>(Config::MAX_VOICES));

	osc.set_frequency(hz);

	osc.set_decay(60000.0f / decay); // convert decay time to decay dB/s
	osc.trigger(pluck_pos, pickup_pos, amplitude);
	filter.reset();
	envelope.trigger();
	pending.valid = false;
}

void Voice::steal(int midi_note, double hz, int velocity) {
	pending = {midi_note, hz, velocity, true};
	envelope.kill();
}

void Voice::release(float release_time) {
	envelope.set_release(release_time);
	envelope.release();
}

void Voice::process(std::span<float> mix_l, std::span<float> mix_r, float cutoff, float resonance) {
	assert(mix_l.size() == tmp.size());
	assert(mix_r.size() == tmp.size());

	// scale cutoff to note pitch: doubles per octave above C4, halves below
	float tracking = std::pow(2.0f, (note - 60) / 12.0f * Config::FILTER_KEYTRACK);

	filter.set_cutoff(cutoff * tracking);
	filter.set_resonance(resonance);

	osc.process(tmp);

	for (int i = 0; i < static_cast<int>(tmp.size()); ++i) {
		float s = filter.process(tmp[i]) * envelope.process();
		mix_l[i] += s * pan_left;
		mix_r[i] += s * pan_right;
	}
}