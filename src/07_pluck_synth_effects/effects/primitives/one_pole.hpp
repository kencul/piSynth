#pragma once

class OnePole {
public:
	// coeff 0.01 means slow smoothing (1% change per sample), 0.99 means fast smoothing (99% change
	// per sample)
	void set_coefficient(float coeff) { this->coeff = coeff; }
	void set_cutoff(float hz);
	void reset(float value) { state = value; }
	float process(float input);

private:
	float coeff {0.0f};
	float state {0.0f};
};
