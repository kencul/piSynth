#include "allpass.hpp"
#include "../../config.hpp"

void Allpass::init(float delay_ms) {
	delay_samples = delay_ms * static_cast<float>(Config::SAMPLE_RATE) / 1000.0f;
	delay.init(static_cast<int>(delay_samples) + 2);
}

float Allpass::process(float input) {
	float delayed = delay.read(delay_samples);

	float feedback = delayed * gain + input;
	delay.write(feedback);

	return delayed + feedback * -gain;
}