#pragma once
#include "../config.hpp"
#include "../effects/primitives/one_pole.hpp"

class SmoothedValue {
public:
	enum class Granularity { PerSample, PerBlock };
	// time_ms: how long it takes to reach a new target (~63% in one time constant)
	explicit SmoothedValue(float time_ms = 20.0f, Granularity g = Granularity::PerSample);

	void set_time(float time_ms);
	void set_target(float target);

	// snap immediately with no smoothing: call after init to avoid zipper on first block
	void reset(float value);

	float next();

private:
	// one pole coefficient from smoothing time: c = e^(-1 / (time_s * sample_rate))
	float time_to_coeff(float time_ms) const;

	OnePole filter;
	float target            = 0.0f;
	Granularity granularity = Granularity::PerSample;
};