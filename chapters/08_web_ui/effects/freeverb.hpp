#pragma once
#include "../common/smoothed_value.hpp"
#include "primitives/allpass.hpp"
#include "primitives/comb.hpp"
#include <span>

class Freeverb {
public:
	void init(float room_size, float cutoff_freq, float mix);
	void process(
	    std::span<float> mix_l, std::span<float> mix_r, float room_size, float cutoff_freq, float mix);

private:
	static constexpr int NUM_COMBS     = 8;
	static constexpr int NUM_ALLPASSES = 4;

	static constexpr std::array<float, NUM_COMBS> COMB_DELAYS_MS = {
	    25.3f, 26.9f, 28.9f, 30.7f, 32.6f, 33.8f, 35.3f, 37.4f};
	static constexpr std::array<float, NUM_ALLPASSES> ALLPASS_DELAYS_MS = {
	    12.6f, 10.0f, 7.7f, 5.0f};
	static constexpr float RIGHT_CHANNEL_OFFSET_MS =
	    5.0f; // small offset to decorrelate left and right channels

	void set_room_size(float room_size);
	void update_cutoff(float cutoff_freq);

	std::array<Comb, NUM_COMBS> combs_l, combs_r;
	std::array<Allpass, NUM_ALLPASSES> allpasses_l, allpasses_r;

	float last_room_size = 0.0f;
	float last_cutoff_freq = 0.0f;

	SmoothedValue mix_smoother {20.0f};
	SmoothedValue cutoff_smoother {20.0f, SmoothedValue::Granularity::PerBlock};
	SmoothedValue room_size_smoother {20.0f, SmoothedValue::Granularity::PerBlock};
};