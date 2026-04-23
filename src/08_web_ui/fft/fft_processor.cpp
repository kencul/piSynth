#include "fft_processor.hpp"
#include "../config.hpp"
#include <algorithm>
#include <cmath>

void FftProcessor::init() {
	setup = pffft_new_setup(FFT_SIZE, PFFFT_REAL);

	window.resize(FFT_SIZE);
	for (int i = 0; i < FFT_SIZE; i++)
		window[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (FFT_SIZE - 1)));

	in_buf.resize(FFT_SIZE);
	out_buf.resize(FFT_SIZE);
	work_buf.resize(FFT_SIZE);
	accum_buf.resize(FFT_SIZE);
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

template <int N>
std::optional<SpectrumMsg> FftProcessor::process(FftAccumulator<N> &acc) {
	if (acc.available() < FFT_SIZE) return std::nullopt;

	acc.read(accum_buf.data(), FFT_SIZE);

	for (int i = 0; i < FFT_SIZE; i++)
		in_buf[i] = accum_buf[i] * window[i];

	pffft_transform_ordered(setup, in_buf.data(), out_buf.data(), work_buf.data(), PFFFT_FORWARD);

	// pffft ordered real output layout:
	//   out[0]        = DC bin (real only)
	//   out[1]        = Nyquist bin (real only)
	//   out[2k], out[2k+1] = re, im of bin k  (k = 1 .. N/2-1)
	const float log_min = std::log10(20.0f);
	const float log_max = std::log10(static_cast<float>(Config::SAMPLE_RATE) / 2.0f);
	const float bin_hz  = static_cast<float>(Config::SAMPLE_RATE) / FFT_SIZE;

	SpectrumMsg msg;
	for (int b = 0; b < OUT_BINS; b++) {
		float t    = static_cast<float>(b) / (OUT_BINS - 1);
		float freq = std::pow(10.0f, log_min + t * (log_max - log_min));
		int k      = std::clamp(static_cast<int>(freq / bin_hz), 1, BIN_COUNT - 1);
		msg.bins[b] = bin_to_db(out_buf[2 * k], out_buf[2 * k + 1]);
	}
	return msg;
}

template std::optional<SpectrumMsg> FftProcessor::process<8192>(FftAccumulator<8192> &);
