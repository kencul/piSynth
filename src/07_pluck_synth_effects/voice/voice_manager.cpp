#include "voice_manager.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

void VoiceManager::init(int period_size) {
	for (auto &v : voices) v.init(period_size);
	cutoff_smoother.reset(params.value(SynthParams::ParamId::FilterCutoff));
	resonance_smoother.reset(params.value(SynthParams::ParamId::FilterResonance));
}

void VoiceManager::handle(const NoteEvent &ev) {
	if (ev.type == NoteEvent::Type::NoteOn) {
		int idx = allocate_voice();
		if (voices[idx].is_active()) {
			voices[idx].steal(ev.note, midi_to_hz(ev.note), ev.velocity);
		} else {
			trigger_note(voices[idx], ev.note, midi_to_hz(ev.note), ev.velocity);
		}
		voice_age[idx] = age_counter++;
	} else {
		float release_time = params.value(SynthParams::ParamId::ReleaseTime);
		for (auto &v : voices) {
			v.try_release(ev.note, release_time);
			v.cancel_pending(ev.note);
		}
	}
}

void VoiceManager::process(std::span<float> mix_l, std::span<float> mix_r) {
	std::fill(mix_l.begin(), mix_l.end(), 0.0f);
	std::fill(mix_r.begin(), mix_r.end(), 0.0f);

	cutoff_smoother.set_target(params.value(SynthParams::ParamId::FilterCutoff));
	resonance_smoother.set_target(params.value(SynthParams::ParamId::FilterResonance));

	float cutoff_hz = cutoff_smoother.next();
	float resonance = resonance_smoother.next();

	for (auto &v : voices) {
		if (!v.is_active()) continue;

		v.process(mix_l, mix_r, cutoff_hz, resonance);

		if (!v.is_idle()) continue;

		if (auto p = v.consume_pending()) { trigger_note(v, p->note, p->hz, p->velocity); }
	}
}

int VoiceManager::allocate_voice() {
	for (int i = 0; i < Config::MAX_VOICES; ++i)
		if (voices[i].is_free()) return i;

	for (int i = 0; i < Config::MAX_VOICES; ++i)
		if (voices[i].is_releasing()) return i;

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