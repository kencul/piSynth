#include "voice_manager.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

void VoiceManager::init(int period_size) {
	mix_l.assign(period_size, 0.0f);
	mix_r.assign(period_size, 0.0f);
	tmp.assign(period_size, 0.0f);
}

void VoiceManager::handle(const NoteEvent &ev) {
	if (ev.type == NoteEvent::Type::NoteOn) {
		int idx = allocate_voice();
		if (voices[idx].active) {
			voices[idx].steal(ev.note, midi_to_hz(ev.note), ev.velocity);
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

void VoiceManager::process(int16_t *buf, int frames, int channels) {
	assert(frames <= mix_l.size());
	assert(frames <= mix_r.size());

	std::fill(mix_l.begin(), mix_l.end(), 0.0f);
	std::fill(mix_r.begin(), mix_r.end(), 0.0f);

	bool any_active = false;
	for (auto &v : voices) {
		if (!v.active) continue;
		any_active = true;
		v.osc.process(tmp.data(), frames);
		for (int i = 0; i < frames; ++i) {
			float s = tmp[i] * v.envelope.process();
			mix_l[i] += s * v.pan_left;
			mix_r[i] += s * v.pan_right;
		}
	}

	if (!any_active) {
		std::memset(buf, 0, frames * channels * sizeof(int16_t));
		return;
	}

	// check idle voices and trigger pending notes or clear and deactivate
	for (auto &v : voices) {
		if (!v.active || !v.envelope.is_idle()) continue;
		if (v.pending.valid) v.trigger(v.pending.note, v.pending.hz, v.pending.velocity);
		else { v.active = false; }
	}

	for (int i = 0; i < frames; ++i) {
		float l = std::tanh(mix_l[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;
		float r = std::tanh(mix_r[i] * Config::SATURATION_DRIVE) / Config::SATURATION_DRIVE;

		float gain = params.value(SynthParams::ParamId::MasterGain);

		buf[i * channels + 0] =
		    static_cast<int16_t>(std::clamp(l * gain, -1.0f, 1.0f) * Config::SAMPLE_SCALE);
		buf[i * channels + 1] =
		    static_cast<int16_t>(std::clamp(r * gain, -1.0f, 1.0f) * Config::SAMPLE_SCALE);
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