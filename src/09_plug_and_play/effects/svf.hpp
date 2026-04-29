#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

class SVF {
public:
	explicit SVF();

	void set_cutoff(float hz);
	void set_resonance(float resonance); // 0.0 (none) to 1.0 (self-oscillation)
	void reset();

	float process(float input);

private:
	void update_coefficients();

	float fast_tanh(float x) const;

	float g {0.0f};
	float k {2.0f};
	float a1 {0.0f};
	float a2 {0.0f};
	float a3 {0.0f};
	float s1 {0.0f};
	float s2 {0.0f};
	float last_cutoff {0.0f};
	float last_resonance {0.0f};
	unsigned int last_sample_rate {0};
};