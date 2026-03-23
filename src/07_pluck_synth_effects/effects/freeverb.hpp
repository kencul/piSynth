#pragma once
#include "primitives/allpass.hpp"
#include "primitives/comb.hpp"
#include <span>

class Freeverb {
public:
	void init();
	void set_room_size(float room_size);
	void set_damping(float cutoff_freq) { this->cutoff_freq = cutoff_freq; }
	void set_wet(float wet) { this->wet = wet; }
	void process(std::span<float> mix_l, std::span<float> mix_r);

private:
	static constexpr int NUM_COMBS     = 8;
	static constexpr int NUM_ALLPASSES = 4;

	static constexpr std::array<float, NUM_COMBS> COMB_DELAYS_MS = {
	    25.3f, 26.9f, 28.9f, 30.7f, 32.6f, 33.8f, 35.3f, 37.4f};
	static constexpr std::array<float, NUM_ALLPASSES> ALLPASS_DELAYS_MS = {
	    12.6f, 10.0f, 7.7f, 5.0f};
	static constexpr float RIGHT_CHANNEL_OFFSET_MS =
	    0.5f; // small offset to decorrelate left and right channels

	std::array<Comb, NUM_COMBS> combs_l, combs_r;
	std::array<Allpass, NUM_ALLPASSES> allpasses_l, allpasses_r;
	float wet {0.3f};
	float cutoff_freq {12000.0f};
	float room_size {0.0f};
};