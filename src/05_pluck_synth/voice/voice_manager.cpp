#include "voice_manager.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

void VoiceManager::handle(const NoteEvent &ev) {
	if (ev.type == NoteEvent::Type::NoteOn) {
		int idx = allocate_voice();
		if (voices[idx].active) {
			voices[idx].steal(ev.note, midi_to_hz(ev.note), ev.velocity);
			std::cout << "Stealing voice " << idx << " for note " << ev.note << "\n";
		} else
			voices[idx].trigger(ev.note, midi_to_hz(ev.note), ev.velocity);
		voice_age[idx] = age_counter++;
	} else {
		for (int i = 0; i < Config::MAX_VOICES; ++i) {
			if (voices[i].active && voices[i].note == ev.note) voices[i].release();
			if (voices[i].pending.valid && voices[i].pending.note == ev.note)
				voices[i].pending.valid = false;
		}
	}
}

void VoiceManager::process(int32_t *buf, int frames, int channels) {
	mix.fill(0.0f);

	int active_count = 0;
	for (auto &v : voices)
		if (v.active) ++active_count;

	if (active_count == 0) {
		std::memset(buf, 0, frames * channels * sizeof(int32_t));
		return;
	}

	for (auto &v : voices) {
		if (!v.active) continue;
		v.osc.process(tmp.data(), frames);
		for (int i = 0; i < frames; ++i) mix[i] += tmp[i] * v.velocity_gain * v.envelope.process();
	}

	// check idle voices and trigger pending notes or clear and deactivate
	for (auto &v : voices) {
		if (!v.active || !v.envelope.is_idle()) continue;
		if (v.pending.valid) v.trigger(v.pending.note, v.pending.hz, v.pending.velocity);
		else {
			v.osc.clear(); // wipe delay line so stale content can't leak
			v.active = false;
		}
	}

	for (int i = 0; i < frames; ++i) {
		// tanh as a safety limiter
		float out = std::tanh(mix[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;

		int32_t sample = static_cast<int32_t>(std::clamp(out, -1.0f, 1.0f) * Config::SAMPLE_SCALE);
		for (int ch = 0; ch < channels; ++ch) buf[i * channels + ch] = sample;
	}
}

int VoiceManager::allocate_voice() {
	for (int i = 0; i < Config::MAX_VOICES; ++i)
		if (!voices[i].active) return i;

	for (int i = 0; i < Config::MAX_VOICES; ++i)
		if (voices[i].active && voices[i].envelope.is_releasing()) return i;

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