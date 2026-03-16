#pragma once
#include "../config.hpp"

class ADSR {
public:
	explicit ADSR(float sample_rate = Config::SAMPLE_RATE);

	void set_attack(float ms);
	void set_decay(float ms);
	void set_sustain(float level);
	void set_release(float ms);

	void trigger(); // call on Note On
	void release(); // call on Note Off
	void kill();

	float process(); // call per sample, returns gain multiplier [0.0, 1.0]
	bool is_idle();  // true when envelope has finished release
	bool is_releasing() const;

private:
	enum class Stage { Idle, Attack, Decay, Sustain, Release, Kill };

	// converts a time in ms to a per-sample increment
	float ms_to_rate(float ms) const;

	Stage stage = Stage::Idle;
	float level = 0.0f; // current envelope value

	float attack_rate;
	float decay_rate;
	float sustain_level;
	float release_rate;
	float kill_rate;

	float sample_rate;
};