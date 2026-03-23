#pragma once
#include "../adsr/adsr.hpp"
#include "../config.hpp"
#include "../effects/svf.hpp"
#include "../osc/osc.hpp"
#include <span>
#include <vector>

class Voice {
public:
	void init(int period_size);
	void trigger(int midi_note,
	             double hz,
	             int velocity,
	             float attack,
	             float decay,
	             float pluck_pos,
	             float pickup_pos);
	void steal(int midi_note, double hz, int velocity);
	void release(float release_time);

	void process(std::span<float> mix_l,
	             std::span<float> mix_r,
	             std::span<const float> cutoff_buf,
	             std::span<const float> resonance_buf);

	Pluck osc {};
	ADSR envelope {};
	SVF filter {};

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