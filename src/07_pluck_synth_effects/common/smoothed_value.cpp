#include "smoothed_value.hpp"
#include <cmath>

SmoothedValue::SmoothedValue(float time_ms, Granularity g, float snap_threshold) :
    granularity(g), snap_threshold(snap_threshold) {
	set_time(time_ms);
}

void SmoothedValue::set_time(float time_ms) { filter.set_coefficient(time_to_coeff(time_ms)); }

void SmoothedValue::set_target(float t) { target = t; }

void SmoothedValue::reset(float value) {
	target = value;
	filter.reset(value);
}

float SmoothedValue::next() {
	float current = filter.process(target);

	// snap to target once close enough to prevent infinite asymptotic tail
	if (std::abs(current - target) < snap_threshold) {
		filter.reset(target);
		return target;
	}

	return current;
}

float SmoothedValue::time_to_coeff(float time_ms) const {
	float rate = granularity == Granularity::PerBlock ?
	    time_ms * 0.001f * Config::SAMPLE_RATE / Config::PERIOD_SIZE :
	    time_ms * 0.001f * Config::SAMPLE_RATE;
	// derived from the definition of a one pole time constant
	return std::exp(-1.0f / rate);
}