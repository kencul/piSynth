#include "chorus.hpp"

Chorus::Chorus() {
	lfo_l.set_shape(LFO::Shape::Sine);
	lfo_r.set_shape(LFO::Shape::Sine);
}

void Chorus::init(float initial_mix, float initial_rate, float initial_depth) {
	int max_samples = static_cast<int>(ms_to_samples(Config::CHORUS_MAX_DELAY_MS)) + 2;
	delay_l.init(max_samples);
	delay_r.init(max_samples);

	mix_smoother.reset(initial_mix);
	depth_smoother.reset((Config::CHORUS_DEPTH_COUPLING / initial_rate) * initial_depth);
}

void Chorus::process(
    std::span<float> mix_l, std::span<float> mix_r, float rate, float depth, float wet) {
	mix_smoother.set_target(wet);

	float target_depth_ms = (Config::CHORUS_DEPTH_COUPLING / rate) * depth;
	depth_smoother.set_target(target_depth_ms);

	lfo_l.set_rate(rate * 1.2f);
	lfo_r.set_rate(rate * 0.8f);

	for (int i = 0; i < static_cast<int>(mix_l.size()); ++i) {
		float current_mix = mix_smoother.next_sample();
		float dry         = 1.0f - current_mix;

		float depth_ms = depth_smoother.next_sample();

		float mod_l = ms_to_samples(Config::CHORUS_LEFT_BASE_MS + lfo_l.process() * depth_ms);
		float mod_r = ms_to_samples(Config::CHORUS_RIGHT_BASE_MS + lfo_r.process() * depth_ms);

		float delayed_l = delay_l.read(mod_l);
		float delayed_r = delay_r.read(mod_r);

		delay_l.write(mix_l[i]);
		delay_r.write(mix_r[i]);

		mix_l[i] = mix_l[i] * dry + delayed_l * current_mix;
		mix_r[i] = mix_r[i] * dry + delayed_r * current_mix;
	}
}

float Chorus::ms_to_samples(float ms) const { return ms * 0.001f * Config::SAMPLE_RATE; }