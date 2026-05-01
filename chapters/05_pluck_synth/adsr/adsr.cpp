#include "adsr.hpp"
#include <algorithm>

ADSR::ADSR(float sample_rate) : sample_rate(sample_rate) {
	set_release(Config::RELEASE_TIME);
	set_attack(Config::ATTACK_TIME);
}

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
	float kill_samples = Config::KILL_MS * 0.001f * sample_rate;
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

float ADSR::ms_to_rate(float ms) const { return 1.0f / (ms * 0.001f * sample_rate); }