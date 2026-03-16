#include "adsr.hpp"
#include <algorithm>

ADSR::ADSR(float sample_rate) : sample_rate(sample_rate) {
	// initialise with defaults from config
	set_attack(Config::DEFAULT_ATTACK);
	set_decay(Config::DEFAULT_DECAY);
	set_sustain(Config::DEFAULT_SUSTAIN);
	set_release(Config::DEFAULT_RELEASE);
}

void ADSR::set_attack(float ms) { attack_rate = ms_to_rate(ms); }
void ADSR::set_decay(float ms) { decay_rate = ms_to_rate(ms); }
void ADSR::set_sustain(float level) { sustain_level = std::clamp(level, 0.0f, 1.0f); }
void ADSR::set_release(float ms) { release_rate = ms_to_rate(ms); }

void ADSR::trigger() {
	// retrigger from current level to avoid click on fast repeated notes
	stage = Stage::Attack;
}

void ADSR::release() {
	if (stage != Stage::Idle) stage = Stage::Release;
}

void ADSR::kill() {
	kill_rate = level / Config::KILL_SAMPLES; // ramp from current level to 0
	stage     = Stage::Kill;
}

float ADSR::process() {
	switch (stage) {
		case Stage::Attack:
			level += attack_rate;
			if (level >= 1.0f) {
				level = 1.0f;
				stage = Stage::Decay;
			}
			break;

		case Stage::Decay:
			level -= decay_rate;
			if (level <= sustain_level) {
				level = sustain_level;
				stage = Stage::Sustain;
			}
			break;

		case Stage::Sustain:
			// level holds until release() is called
			break;

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

bool ADSR::is_idle() { return stage == Stage::Idle; }

float ADSR::ms_to_rate(float ms) const {
	// rate = samples needed to travel 0→1 over the given duration
	return 1.0f / (ms * 0.001f * sample_rate);
}

bool ADSR::is_releasing() const { return stage == Stage::Release; }