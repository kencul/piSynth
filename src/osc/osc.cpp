#include "osc.hpp"
#include <numbers> // for pi_v

void Pluck::set_frequency(double hz) {
	frequency = hz;
	// -0.5 compensates for the half-sample delay of the one-pole averaging filter; omitting it
	// causes flat pitch
	delay_len      = static_cast<float>(Config::SAMPLE_RATE / hz - 0.5f);
	half_delay_len = delay_len / 2.0f;
}

void Pluck::set_decay(float decay_db_per_sec) {
	float f0 = static_cast<float>(frequency);
	float fs = static_cast<float>(Config::SAMPLE_RATE);
	// G: per-cycle gain target derived from the requested dB/s decay rate
	// A: one-pole filter gain at f0; dividing by A compensates for its frequency-dependent
	// attenuation so the measured decay matches the requested rate. 0.4995 cap prevents waveguide
	// instability.
	float G       = std::pow(10.0f, -decay_db_per_sec / (20.0f * f0));
	float A       = std::cos(std::numbers::pi_v<float> * f0 / fs);
	feedback_gain = std::min((G / A) * 0.5f, 0.4995f);
}

void Pluck::trigger(float pluck_pos, float pickup_pos, float amplitude) {
	clear();
	pickup_pos_norm = pickup_pos;

	int string_len = static_cast<int>(std::ceil(half_delay_len));
	if (string_len < 2) string_len = 2;

	int peak = std::max(1, std::min(static_cast<int>(pluck_pos * string_len), string_len - 1));

	for (int i = 0; i < peak; ++i)
		delay_line[i] = amplitude * static_cast<float>(i) / static_cast<float>(peak);
	for (int i = peak; i < string_len; ++i)
		delay_line[i] =
		    amplitude * static_cast<float>(string_len - i) / static_cast<float>(string_len - peak);

	// mirror the first half into the second half to seed both traveling wave directions
	for (int i = 0; i < string_len; ++i)
		delay_line[string_len + i] = delay_line[string_len - 1 - i];

	for (int i = string_len * 2; i < MAX_DELAY; ++i) delay_line[i] = 0.0f;

	write_pos = string_len;
	prev      = 0.0f;
}

void Pluck::clear() {
	delay_line.fill(0.0f);
	prev      = 0.0f;
	dc_x      = 0.0f;
	dc_y      = 0.0f;
	write_pos = 0;
}

float Pluck::interpolate_delay_line(float read_idx_float) const {
	// Ensure `read_idx_float` is positive for modulo arithmetic.
	// fmod handles negatives cleanly and signals intent
	read_idx_float = std::fmod(read_idx_float, static_cast<float>(MAX_DELAY));
	if (read_idx_float < 0.0f) read_idx_float += static_cast<float>(MAX_DELAY);

	int idx0   = static_cast<int>(read_idx_float) & (MAX_DELAY - 1);
	int idx1   = (idx0 + 1) & (MAX_DELAY - 1);
	float frac = read_idx_float - std::floor(read_idx_float);
	return delay_line[idx0] + frac * (delay_line[idx1] - delay_line[idx0]);
}

void Pluck::snapshot(WaveguideSnapshot &out) const {
	out.fret_pos   = half_delay_len / (MAX_DELAY * 0.5f);
	out.pickup_pos = pickup_pos_norm;
	out.active     = true;

	// write_pos is the next-write slot; wp-1 is the most recent valid sample
	float wp = static_cast<float>(write_pos) - 1.0f;
	for (int i = 0; i < WaveguideSnapshot::POINTS; ++i) {
		float x = static_cast<float>(i) / static_cast<float>(WaveguideSnapshot::POINTS - 1)
		    * half_delay_len;
		float right_going   = interpolate_delay_line(wp - x);
		float left_going    = interpolate_delay_line(wp - 2.0f * half_delay_len + x);
		out.displacement[i] = right_going - left_going;
	}
}

void Pluck::process(std::span<float> buf) {
	for (int i = 0; i < static_cast<int>(buf.size()); ++i) {
		// Calculate delay for the forward-traveling wave to reach `pickup_pos_norm`
		float fwd_delay_samps = pickup_pos_norm * half_delay_len;
		float fwd_read_pos    = static_cast<float>(write_pos) - fwd_delay_samps;
		float sample_fwd      = interpolate_delay_line(fwd_read_pos);

		// Calculate delay for the backward-traveling wave to reach `pickup_pos_norm`
		// This wave has traveled the full string length (half_delay_len) and
		// then returned from the bridge by `(1.0f - pickup_pos_norm)` of the string length
		float bwd_delay_samps = half_delay_len + (1.0f - pickup_pos_norm) * half_delay_len;
		float bwd_read_pos    = static_cast<float>(write_pos) - bwd_delay_samps;
		float sample_bwd      = interpolate_delay_line(bwd_read_pos);

		// combine forward and backward waves to simulate a pickup
		float output = (sample_fwd + sample_bwd) * 0.5f;

		float read_pos  = static_cast<float>(write_pos) - delay_len;
		float from_line = interpolate_delay_line(read_pos);

		// one-pole LPF + feedback gain simulates string damping
		delay_line[write_pos] = (from_line + prev) * feedback_gain;
		prev                  = from_line;
		write_pos             = (write_pos + 1) & (MAX_DELAY - 1);

		// remove DC: leaky integrator tracks the mean, subtract it
		float y = output - dc_x + 0.9999f * dc_y;
		dc_x    = output;
		dc_y    = y;
		buf[i]  = y;
	}
}