#pragma once
#include "../adsr/adsr.hpp"
#include "../config.hpp"
#include "../midi/synth_params.hpp"
#include "../osc/osc.hpp"
#include <vector>
#include <span>

class Voice {
public:
	void init(int period_size);
	void trigger(int midi_note, double hz, int velocity);
	void steal(int midi_note, double hz, int velocity);
	void release();

	void process(std::span<float> mix_l, std::span<float> mix_r);

	Pluck osc {Config::SAMPLE_RATE};
	ADSR envelope {Config::SAMPLE_RATE};

	SynthParams *params = nullptr;

	int note    = -1;
	bool active = false;

	struct PendingNote {
		int note;
		double hz;
		int velocity;
		bool valid = false;
	} pending;

	float pan_left  = 1.0f;
	float pan_right = 1.0f;

private:
	// scratch buffer for osc output before pan is applied
	std::vector<float> tmp;
};