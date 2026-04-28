// fft/fft_accumulator.hpp
#pragma once
#include <array>
#include <atomic>

// SPSC float ring buffer for shipping audio samples to the FFT thread.
// Write one mono sample per frame from the audio thread.
template <int N> class FftAccumulator {
public:
	// Returns false and drops the sample if full.
	bool write(float sample) {
		size_t w    = write_pos.load(std::memory_order_relaxed);
		size_t next = (w + 1) % N;
		if (next == read_pos.load(std::memory_order_acquire)) return false;
		buf[w] = sample;
		write_pos.store(next, std::memory_order_release);
		return true;
	}

	// Copies up to `count` samples into `out`. Returns number actually copied.
	int read(float *out, int count) {
		int n    = 0;
		size_t r = read_pos.load(std::memory_order_relaxed);
		while (n < count && r != write_pos.load(std::memory_order_acquire)) {
			out[n++] = buf[r];
			r        = (r + 1) % N;
		}
		read_pos.store(r, std::memory_order_release);
		return n;
	}

	int available() const {
		size_t w = write_pos.load(std::memory_order_acquire);
		size_t r = read_pos.load(std::memory_order_acquire);
		return static_cast<int>((w - r + N) % N);
	}

private:
	std::array<float, N> buf {};
	std::atomic<size_t> write_pos {0};
	std::atomic<size_t> read_pos {0};
};