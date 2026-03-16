#pragma once
#include "../config.hpp"

class ADSR {
public:
	explicit ADSR(float sample_rate = Config::SAMPLE_RATE);

	void set_attack(float ms);
	void set_release(float ms);

	void trigger(); // immediately holds at 1.0
	void release(); // starts fade to 0
	void kill();    // fast ramp to 0 for voice stealing

	float process();
	bool is_idle() const;
	bool is_releasing() const;

private:
	enum class Stage { Idle, Attack, Sustain, Release, Kill };

	float ms_to_rate(float ms) const;

	Stage stage        = Stage::Idle;
	float level        = 0.0f;
	float attack_rate  = 0.0f;
	float release_rate = 0.0f;
	float kill_rate    = 0.0f;
	float sample_rate;
};