#pragma once

// Attack-Sustain-Release envelope. A separate Decay stage is intentionally omitted:
// amplitude decay is handled by the waveguide feedback_gain in Pluck, so this envelope
// only needs to gate the voice on (attack), hold it (sustain), and release it.
class ADSR {
public:
	ADSR() = default;

	void set_attack(float ms);
	void set_release(float ms);

	void trigger();
	void release();
	void kill(); // fast ramp to 0 for voice stealing

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