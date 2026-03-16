#include "voice_manager.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

void VoiceManager::handle(const NoteEvent &ev) {
	if (ev.type == NoteEvent::Type::NoteOn) {
		int idx = allocate_voice();

		if (voices[idx].active) {
			// voice is in use, fade it out first, then trigger the new note
			voices[idx].steal(ev.note, midi_to_hz(ev.note), ev.velocity);
			std::cout << "Stealing voice " << idx << " for note " << ev.note << "\n";
		} else
			voices[idx].trigger(ev.note, midi_to_hz(ev.note), ev.velocity);

		voice_age[idx] = age_counter++;
	} else {
		// release all voices playing the incoming note
		for (int i = 0; i < Config::MAX_VOICES; ++i) {
			if (voices[i].active && voices[i].note == ev.note) voices[i].release();

			// NoteOff arrived before pending note fired
			if (voices[i].pending.valid && voices[i].pending.note == ev.note)
				voices[i].pending.valid = false;
		}
	}
}

void VoiceManager::process(int32_t *buf, int frames, int channels) {
	float mix[Config::PERIOD_SIZE] = {};

	int active_count = 0;
	for (auto &v : voices)
		if (v.active) ++active_count;

	if (active_count == 0) {
		std::memset(buf, 0, frames * channels * sizeof(int32_t));
		return;
	}

	static constexpr float VOICE_GAIN = 1.0f / static_cast<float>(Config::MAX_VOICES);

	float tmp[Config::PERIOD_SIZE] = {};

	for (auto &v : voices) {
		if (!v.active) continue;

		v.osc.process(tmp, frames);

		float gain = VOICE_GAIN * v.velocity_gain;
		for (int i = 0; i < frames; ++i) mix[i] += tmp[i] * gain * v.envelope.process();
	}

	// check idle voices and trigger pending notes
	for (auto &v : voices) {
		if (!v.active || !v.envelope.is_idle()) continue;

		if (v.pending.valid) v.trigger(v.pending.note, v.pending.hz, v.pending.velocity);
		else
			v.active = false;
	}

	for (int i = 0; i < frames; ++i) {
		int32_t sample =
		    static_cast<int32_t>(std::clamp(mix[i], -1.0f, 1.0f) * Config::SAMPLE_SCALE);
		for (int ch = 0; ch < channels; ++ch) buf[i * channels + ch] = sample;
	}
}

int VoiceManager::allocate_voice() {
	// prefer a fully inactive voice first
	for (int i = 0; i < Config::MAX_VOICES; ++i)
		if (!voices[i].active) return i;

	// prefer stealing a releasing voice over an active one — less audible
	for (int i = 0; i < Config::MAX_VOICES; ++i)
		if (voices[i].active && voices[i].envelope.is_releasing()) return i;

	// all voices sustaining — steal the oldest
	int oldest_idx = 0;
	int oldest_age = voice_age[0];
	for (int i = 1; i < Config::MAX_VOICES; ++i) {
		if (voice_age[i] < oldest_age) {
			oldest_age = voice_age[i];
			oldest_idx = i;
		}
	}
	return oldest_idx;
}

double VoiceManager::midi_to_hz(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }