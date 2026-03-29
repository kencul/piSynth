#include "ping_pong.hpp"

void PingPong::init(float initial_delay_ms, float initial_feedback, float initial_mix) {
	int max_samples = static_cast<int>(ms_to_samples(Config::PING_PONG_MAX_DELAY_MS)) + 2;
	delay_l.init(max_samples);
	delay_r.init(max_samples);

	filter_l.set_cutoff(Config::PING_PONG_FEEDBACK_CUTOFF_HZ);
	filter_r.set_cutoff(Config::PING_PONG_FEEDBACK_CUTOFF_HZ);

	delay_smoother.reset(initial_delay_ms);
	feedback_smoother.reset(initial_feedback);
	mix_smoother.reset(initial_mix);
}

void PingPong::process(
    std::span<float> mix_l, std::span<float> mix_r, float delay_ms, float feedback, float mix) {
	delay_smoother.set_target(delay_ms);
	feedback_smoother.set_target(feedback);
	mix_smoother.set_target(mix);

	float current_feedback = feedback_smoother.next();

	for (int i = 0; i < static_cast<int>(mix_l.size()); ++i) {
		float mono_in = (mix_l[i] + mix_r[i]) * 0.5f;

		float current_delay = ms_to_samples(delay_smoother.next());

		float delayed_l = delay_l.read(current_delay);
		float delayed_r = delay_r.read(current_delay);

		float current_mix = mix_smoother.next();

		// apply a one-pole lowpass to the feedback path to prevent harshness
		delayed_l = filter_l.process(delayed_l);
		delayed_r = filter_r.process(delayed_r);

		delay_l.write(mono_in + delayed_r * current_feedback);
		delay_r.write(delayed_l * current_feedback);

		mix_l[i] = mix_l[i] * (1.0f - current_mix) + delayed_l * current_mix;
		mix_r[i] = mix_r[i] * (1.0f - current_mix) + delayed_r * current_mix;
	}
}