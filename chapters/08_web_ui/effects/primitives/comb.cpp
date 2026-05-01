#include "comb.hpp"
#include "../../config.hpp"

void Comb::init(float delay_ms, float cutoff_hz) {
	this->delay_samples = delay_ms * static_cast<float>(Config::SAMPLE_RATE) / 1000.0f;
	delay.init(static_cast<int>(delay_samples) + 2);
	filter.set_cutoff(cutoff_hz);
}

float Comb::process(float input) {
	float delayed  = delay.read(delay_samples);
	float filtered = filter.process(delayed);
	float feedback = filtered * gain + input;
	delay.write(feedback);
	return delayed;
}

void Comb::set_cutoff(float cutoff_hz) { filter.set_cutoff(cutoff_hz); }