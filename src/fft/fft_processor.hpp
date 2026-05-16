// fft/fft_processor.hpp
#pragma once
#include "../config.hpp"
#include "../web/messages.hpp"
#include "fft_accumulator.hpp"
#include "pffft.h"
#include <optional>
#include <vector>

class FftProcessor {
public:
	static constexpr int FFT_SIZE  = Config::FFT_SIZE;
	static constexpr int BIN_COUNT = FFT_SIZE / 2;
	static constexpr int OUT_BINS  = Config::FFT_OUT_BINS;
	static constexpr int HOP_SIZE  = FFT_SIZE / 2; // 50% overlap

	void init();
	void destroy();
	void reset();

	// Call from the uWS thread. Returns a message if a full window was ready.
	template <int N> std::optional<SpectrumMsg> process(FftAccumulator<N> &acc);

private:
	void apply_hann(float *buf, int n);
	float bin_to_db(float re, float im) const;

	PFFFT_Setup *setup = nullptr;
	std::vector<float> window;    // Hann window coefficients
	std::vector<float> in_buf;   // windowed input, pffft-aligned
	std::vector<float> out_buf;  // pffft output, interleaved re/im
	std::vector<float> work_buf; // pffft scratch space

	std::array<float, FFT_SIZE> overlap_buf {}; // last FFT_SIZE samples, circular
	int overlap_write = 0;                       // write head into overlap_buf
	int hop_counter   = 0;                       // samples since last FFT frame
};