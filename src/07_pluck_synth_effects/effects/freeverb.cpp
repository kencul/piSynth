#include "freeverb.hpp"
#include <algorithm>

void Freeverb::init() {
	for (int i = 0; i < NUM_COMBS; ++i) {
		combs_l[i].init(COMB_DELAYS_MS[i], cutoff_freq);
		combs_r[i].init(COMB_DELAYS_MS[i] + RIGHT_CHANNEL_OFFSET_MS, cutoff_freq);
	}

	for (int i = 0; i < NUM_ALLPASSES; ++i) {
		allpasses_l[i].init(ALLPASS_DELAYS_MS[i]);
		allpasses_r[i].init(ALLPASS_DELAYS_MS[i] + RIGHT_CHANNEL_OFFSET_MS);
	}

	set_room_size(0.5f); // default room size
}

void Freeverb::process(std::span<float> mix_l, std::span<float> mix_r) {
	for (size_t i = 0; i < mix_l.size(); ++i) {
		float input_l = mix_l[i];
		float input_r = mix_r[i];

		float out_l = 0.0f;
		float out_r = 0.0f;

		for (int j = 0; j < NUM_COMBS; ++j) {
			out_l += combs_l[j].process(input_l);
			out_r += combs_r[j].process(input_r);
		}

		out_l /= static_cast<float>(NUM_COMBS);
		out_r /= static_cast<float>(NUM_COMBS);

		for (int j = 0; j < NUM_ALLPASSES; ++j) {
			out_l = allpasses_l[j].process(out_l);
			out_r = allpasses_r[j].process(out_r);
		}

		mix_l[i] = input_l * (1.0f - wet) + out_l * wet;
		mix_r[i] = input_r * (1.0f - wet) + out_r * wet;
	}
}

void Freeverb::set_room_size(float room_size) {
	float clamped_room_size = std::clamp(room_size, 0.0f, 1.0f);
	if (clamped_room_size == this->room_size) return;
	this->room_size = clamped_room_size;
	float feedback  = std::clamp(0.68f + clamped_room_size * 0.28f, 0.0f, 0.99f);
	for (int i = 0; i < NUM_COMBS; ++i) {
		combs_l[i].set_gain(feedback);
		combs_r[i].set_gain(feedback);
	}
}