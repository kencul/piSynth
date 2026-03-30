#include "freeverb.hpp"
#include <algorithm>

void Freeverb::init(float room_size, float cutoff_freq, float mix) {
	room_size_smoother.reset(room_size);
	cutoff_smoother.reset(cutoff_freq);
	mix_smoother.reset(mix);

	last_room_size   = room_size;
	last_cutoff_freq = cutoff_freq;

	for (int i = 0; i < NUM_COMBS; ++i) {
		combs_l[i].init(COMB_DELAYS_MS[i], cutoff_freq);
		combs_r[i].init(COMB_DELAYS_MS[i] + RIGHT_CHANNEL_OFFSET_MS, cutoff_freq);
	}

	for (int i = 0; i < NUM_ALLPASSES; ++i) {
		allpasses_l[i].init(ALLPASS_DELAYS_MS[i]);
		allpasses_r[i].init(ALLPASS_DELAYS_MS[i] + RIGHT_CHANNEL_OFFSET_MS);
	}
}

void Freeverb::process(
    std::span<float> mix_l, std::span<float> mix_r, float room_size, float cutoff_freq, float mix) {
	mix_smoother.set_target(mix);

	room_size_smoother.set_target(room_size);
	float smoothed_room_size = room_size_smoother.next_block();

	if (last_room_size != smoothed_room_size) {
		set_room_size(smoothed_room_size);
		last_room_size = smoothed_room_size;
	}

	cutoff_smoother.set_target(std::clamp(cutoff_freq, 20.0f, Config::SAMPLE_RATE * 0.49f));
	float smoothed_cutoff = cutoff_smoother.next_block();
	if (last_cutoff_freq != smoothed_cutoff) {
		update_cutoff(smoothed_cutoff);
		last_cutoff_freq = smoothed_cutoff;
	}

	for (size_t i = 0; i < mix_l.size(); ++i) {
		float input_mono = (mix_l[i] + mix_r[i]) * 0.5f;

		float out_l = 0.0f;
		float out_r = 0.0f;

		float wet = mix_smoother.next_sample();

		for (int j = 0; j < NUM_COMBS; ++j) {
			out_l += combs_l[j].process(input_mono);
			out_r += combs_r[j].process(input_mono);
		}

		out_l /= static_cast<float>(NUM_COMBS);
		out_r /= static_cast<float>(NUM_COMBS);

		for (int j = 0; j < NUM_ALLPASSES; ++j) {
			out_l = allpasses_l[j].process(out_l);
			out_r = allpasses_r[j].process(out_r);
		}

		mix_l[i] = mix_l[i] * (1.0f - wet) + out_l * wet;
		mix_r[i] = mix_r[i] * (1.0f - wet) + out_r * wet;
	}
}

void Freeverb::set_room_size(float room_size) {
	float clamped_room_size = std::clamp(room_size, 0.0f, 1.0f);

	float feedback = std::clamp(0.68f + clamped_room_size * 0.28f, 0.0f, 0.99f);
	for (int i = 0; i < NUM_COMBS; ++i) {
		combs_l[i].set_gain(feedback);
		combs_r[i].set_gain(feedback);
	}
}

void Freeverb::update_cutoff(float cutoff_freq) {
	for (int i = 0; i < NUM_COMBS; ++i) {
		combs_l[i].set_cutoff(cutoff_freq);
		combs_r[i].set_cutoff(cutoff_freq);
	}
}