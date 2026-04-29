#include "adsr.hpp"
#include "../config.hpp"
#include <algorithm>

void ADSR::set_attack(float ms) { attack_rate = ms_to_rate(ms); }
void ADSR::set_release(float ms) { release_rate = ms_to_rate(ms); }

void ADSR::trigger() {
	level = 0.0f;
	stage = Stage::Attack;
}

void ADSR::release() {
	if (stage != Stage::Idle) stage = Stage::Release;
}

void ADSR::kill() {
	// convert ms to a per-sample decrement from current level to 0
	float kill_samples = Config::KILL_MS * 0.001f * Config::SAMPLE_RATE;
	kill_rate          = level / kill_samples;
	stage              = Stage::Kill;
}

float ADSR::process() {
	switch (stage) {
		case Stage::Attack:
			level += attack_rate;
			if (level >= 1.0f) {
				level = 1.0f;
				stage = Stage::Sustain;
			}
			break;

		case Stage::Sustain: break;

		case Stage::Release:
			level -= release_rate;
			if (level <= 0.0f) {
				level = 0.0f;
				stage = Stage::Idle;
			}
			break;

		case Stage::Kill:
			level -= kill_rate;
			if (level <= 0.0f) {
				level = 0.0f;
				stage = Stage::Idle;
			}
			break;

		case Stage::Idle: break;
	}
	return level;
}

bool ADSR::is_idle() const { return stage == Stage::Idle; }
bool ADSR::is_releasing() const { return stage == Stage::Release; }
bool ADSR::is_killing() const { return stage == Stage::Kill; }

float ADSR::ms_to_rate(float ms) const { return 1.0f / (ms * 0.001f * Config::SAMPLE_RATE); }

void ADSR::reset() {
	stage = Stage::Idle;
	level = 0.0f;
}