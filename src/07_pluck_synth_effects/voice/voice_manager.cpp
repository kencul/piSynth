#include "voice_manager.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

void VoiceManager::init(int period_size) {
	for (auto &v : voices) v.init(period_size);
	cutoff_buf.resize(period_size);
	resonance_buf.resize(period_size);
	cutoff_smoother.reset(params.value(SynthParams::ParamId::FilterCutoff));
	resonance_smoother.reset(params.value(SynthParams::ParamId::FilterResonance));
}

void VoiceManager::handle(const NoteEvent &ev) {
	if (ev.type == NoteEvent::Type::NoteOn) {
		int idx = allocate_voice();
		if (voices[idx].active) {
			voices[idx].steal(ev.note, midi_to_hz(ev.note), ev.velocity);
		} else {
			trigger_note(voices[idx], ev.note, midi_to_hz(ev.note), ev.velocity);
		}
		voice_age[idx] = age_counter++;
	} else {
		for (int i = 0; i < Config::MAX_VOICES; ++i) {
			if (voices[i].active && voices[i].note == ev.note)
				voices[i].release(params.value(SynthParams::ParamId::ReleaseTime));
			if (voices[i].pending.valid && voices[i].pending.note == ev.note)
				voices[i].pending.valid = false;
		}
	}
}

void VoiceManager::process(std::span<float> mix_l, std::span<float> mix_r) {
	std::fill(mix_l.begin(), mix_l.end(), 0.0f);
	std::fill(mix_r.begin(), mix_r.end(), 0.0f);

	cutoff_smoother.set_target(params.value(SynthParams::ParamId::FilterCutoff));
	resonance_smoother.set_target(params.value(SynthParams::ParamId::FilterResonance));

	// advance smoothers once per sample into buffers
	for (int i = 0; i < static_cast<int>(mix_l.size()); ++i) {
		cutoff_buf[i]    = cutoff_smoother.next();
		resonance_buf[i] = resonance_smoother.next();
	}

	for (auto &v : voices) {
		if (!v.active) continue;

		v.process(mix_l, mix_r, cutoff_buf, resonance_buf);

		if (!v.envelope.is_idle()) continue;

		if (v.pending.valid) {
			trigger_note(v, v.pending.note, v.pending.hz, v.pending.velocity);
		} else {
			v.active = false;
		}
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

void VoiceManager::trigger_note(Voice &voice, int midi_note, double hz, int velocity) {
	float attack     = params.value(SynthParams::ParamId::AttackTime);
	float decay      = params.value(SynthParams::ParamId::DecayTime);
	float pluck_pos  = params.value(SynthParams::ParamId::PluckPos);
	float pickup_pos = params.value(SynthParams::ParamId::PickupPos);
	voice.trigger(midi_note, hz, velocity, attack, decay, pluck_pos, pickup_pos);
}