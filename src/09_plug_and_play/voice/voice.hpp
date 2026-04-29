#pragma once
#include "../adsr/adsr.hpp"
#include "../common/waveguide_snapshot.hpp"
#include "../config.hpp"
#include "../effects/svf.hpp"
#include "../osc/osc.hpp"
#include <optional>
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

	bool is_free() const;
	bool is_releasing() const;

	bool is_active() const;
	bool is_idle() const;

	bool try_release(int midi_note, float release_time);
	void cancel_pending(int midi_note);

	struct PendingNote {
		int note;
		double hz;
		int velocity;
	};
	std::optional<PendingNote> consume_pending();

	void process(std::span<float> mix_l, std::span<float> mix_r, float cutoff_hz, float resonance);
	void snapshot(WaveguideSnapshot &out) const;

	void reset();

private:
	// scratch buffer for osc output before pan is applied
	std::vector<float> tmp;

	std::optional<PendingNote> pending;

	void release(float release_time);
	Pluck osc {};
	ADSR envelope {};
	SVF filter {};

	int note    = -1;
	bool active = false;

	float pan_left  = 1.0f;
	float pan_right = 1.0f;
};