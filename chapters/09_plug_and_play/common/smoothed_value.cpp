#include "smoothed_value.hpp"
#include <cassert>
#include <cmath>

SmoothedValue::SmoothedValue(float time_ms, Granularity g, float snap_threshold) :
    time_ms(time_ms), granularity(g), snap_threshold(snap_threshold) {}

void SmoothedValue::set_time(float time_ms) {
	this->time_ms = time_ms;
	filter.set_coefficient(time_to_coeff(time_ms));
}

void SmoothedValue::set_target(float t) { target = t; }

void SmoothedValue::reset(float value) {
	filter.set_coefficient(time_to_coeff(time_ms));
	target = value;
	filter.reset(value);
}

float SmoothedValue::next_sample() {
	assert(granularity == Granularity::PerSample);
	return next();
}

float SmoothedValue::next_block() {
	assert(granularity == Granularity::PerBlock);
	return next();
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