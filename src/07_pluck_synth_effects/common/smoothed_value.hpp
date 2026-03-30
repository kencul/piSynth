#pragma once
#include "../config.hpp"
#include "../effects/primitives/one_pole.hpp"

class SmoothedValue {
public:
	enum class Granularity { PerSample, PerBlock };
	// time_ms: how long it takes to reach a new target (~63% in one time constant)
	explicit SmoothedValue(float time_ms        = 20.0f,
	                       Granularity g        = Granularity::PerSample,
	                       float snap_threshold = 1e-4f);

	void set_time(float time_ms);
	void set_target(float target);

	// snap immediately with no smoothing: call after init to avoid zipper on first block
	void reset(float value);

	float next_sample();
	float next_block();

private:
	// one pole coefficient from smoothing time: c = e^(-1 / (time_s * sample_rate))
	float time_to_coeff(float time_ms) const;

	float next();

	OnePole filter;
	float target            = 0.0f;
	float time_ms           = 20.0f;
	Granularity granularity = Granularity::PerSample;
	float snap_threshold    = 1e-4f;
};