#pragma once

class ADSR {
public:
	ADSR() = default;

	void set_attack(float ms);
	void set_release(float ms);

	void trigger(); // starts attack
	void release(); // starts fade to 0
	void kill();    // fast ramp to 0 for voice stealing

	float process();
	bool is_idle() const;
	bool is_releasing() const;
	bool is_killing() const;

	void reset();

private:
	enum class Stage { Idle, Attack, Sustain, Release, Kill };

	float ms_to_rate(float ms) const;

	Stage stage        = Stage::Idle;
	float level        = 0.0f;
	float attack_rate  = 0.0f;
	float release_rate = 0.0f;
	float kill_rate    = 0.0f;
};