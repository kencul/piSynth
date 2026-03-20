#include "chorus.hpp"

Chorus::Chorus() {
	lfo_l.set_shape(LFO::Shape::Sine);
	lfo_r.set_shape(LFO::Shape::Sine);
}

void Chorus::init(int /*period_size*/) {
	int max_samples = static_cast<int>(ms_to_samples(Config::CHORUS_MAX_DELAY_MS)) + 2;
	delay_l.init(max_samples);
	delay_r.init(max_samples);
}

void Chorus::process(
    std::span<float> mix_l, std::span<float> mix_r, float rate, float depth, float wet) {
	if (wet <= 0.0f || rate < 0.01f) return; // avoid unnecessary processing and extreme modulation

	float depth_ms = (Config::CHORUS_DEPTH_COUPLING / rate) * depth;

	lfo_l.set_rate(rate * 1.2f);
	lfo_r.set_rate(rate * 0.8f);

	float dry = 1.0f - wet;

	for (int i = 0; i < static_cast<int>(mix_l.size()); ++i) {
		float mod_l = ms_to_samples(Config::CHORUS_LEFT_BASE_MS + lfo_l.process() * depth_ms);
		float mod_r = ms_to_samples(Config::CHORUS_RIGHT_BASE_MS + lfo_r.process() * depth_ms);

		float delayed_l = delay_l.read(mod_l);
		float delayed_r = delay_r.read(mod_r);

		delay_l.write(mix_l[i]);
		delay_r.write(mix_r[i]);

		mix_l[i] = mix_l[i] * dry + delayed_l * wet;
		mix_r[i] = mix_r[i] * dry + delayed_r * wet;
	}
}

float Chorus::ms_to_samples(float ms) const { return ms * 0.001f * Config::SAMPLE_RATE; }