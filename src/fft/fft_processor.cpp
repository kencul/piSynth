#include "fft_processor.hpp"
#include "../config.hpp"
#include <algorithm>
#include <cmath>

void FftProcessor::init() {
	setup = pffft_new_setup(FFT_SIZE, PFFFT_REAL);

	window.resize(FFT_SIZE);

	// Blackman-Harris window
	for (int i = 0; i < FFT_SIZE; i++) {
		float a0 = 0.35875f, a1 = 0.48829f, a2 = 0.14128f, a3 = 0.01168f;
		float t   = 2.0f * std::numbers::pi_v<float> * i / (FFT_SIZE - 1);
		window[i] = a0 - a1 * std::cos(t) + a2 * std::cos(2 * t) - a3 * std::cos(3 * t);
	}

	in_buf.resize(FFT_SIZE);
	out_buf.resize(FFT_SIZE);
	work_buf.resize(FFT_SIZE);
}

void FftProcessor::destroy() {
	if (setup) {
		pffft_destroy_setup(setup);
		setup = nullptr;
	}
}

float FftProcessor::bin_to_db(float re, float im) const {
	float mag = std::sqrt(re * re + im * im) * 2.0f / FFT_SIZE;
	return mag < 1e-10f ? -80.0f : 20.0f * std::log10(mag);
}

template <int N> std::optional<SpectrumMsg> FftProcessor::process(FftAccumulator<N> &acc) {
	std::optional<SpectrumMsg> result;

	// Drain all available samples into the internal overlap buffer and fire
	// one FFT every HOP = FFT_SIZE/2 samples. This makes frame alignment
	// deterministic regardless of when the caller invokes process().
	float chunk[256];
	int   n;
	while ((n = acc.read(chunk, 256)) > 0) {
		for (int i = 0; i < n; ++i) {
			overlap_buf[overlap_write] = chunk[i];
			overlap_write              = (overlap_write + 1) % FFT_SIZE;
			if (++hop_counter >= FFT_SIZE / 2) {
				hop_counter = 0;
				// Copy the last FFT_SIZE samples (with ring-buffer wraparound)
				// into in_buf, applying the Blackman-Harris window.
				for (int k = 0; k < FFT_SIZE; ++k) {
					int src   = (overlap_write - FFT_SIZE + k + FFT_SIZE) % FFT_SIZE;
					in_buf[k] = overlap_buf[src] * window[k];
				}
				pffft_transform_ordered(setup, in_buf.data(), out_buf.data(),
				                        work_buf.data(), PFFFT_FORWARD);

				// pffft ordered real output layout:
				//   out[0]             = DC bin (real only)
				//   out[1]             = Nyquist bin (real only)
				//   out[2k], out[2k+1] = re, im of bin k  (k = 1 .. N/2-1)
				const float log_min = std::log10(60.0f);
				const float log_max =
				    std::log10(static_cast<float>(Config::SAMPLE_RATE) / 2.0f);
				const float bin_hz = static_cast<float>(Config::SAMPLE_RATE) / FFT_SIZE;
				SpectrumMsg msg;
				for (int b = 0; b < OUT_BINS; b++) {
					float t       = static_cast<float>(b) / (OUT_BINS - 1);
					float freq    = std::pow(10.0f, log_min + t * (log_max - log_min));
					float bin_idx = std::clamp(freq / bin_hz, 1.0f,
					                           static_cast<float>(BIN_COUNT - 1));
					int   k0      = static_cast<int>(bin_idx);
					int   k1      = std::min(k0 + 1, BIN_COUNT - 1);
					float fract   = bin_idx - static_cast<float>(k0);
					float db0     = bin_to_db(out_buf[2 * k0], out_buf[2 * k0 + 1]);
					float db1     = bin_to_db(out_buf[2 * k1], out_buf[2 * k1 + 1]);
					msg.bins[b]   = db0 * (1.0f - fract) + db1 * fract;
				}
				result = msg; // overwrite — caller receives the most recent frame
			}
		}
	}
	return result;
}

void FftProcessor::reset() {
	overlap_buf.fill(0.0f);
	overlap_write = 0;
	hop_counter   = 0;
	std::fill(out_buf.begin(), out_buf.end(), 0.0f);
}

template std::optional<SpectrumMsg>
FftProcessor::process<Config::FFT_ACC_SIZE>(FftAccumulator<Config::FFT_ACC_SIZE> &);
