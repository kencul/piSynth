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

	// Copies up to `count` samples into `out` without advancing the read pointer.
	int peek(float *out, int count) const {
		int n    = 0;
		size_t r = read_pos.load(std::memory_order_relaxed);
		while (n < count && r != write_pos.load(std::memory_order_acquire)) {
			out[n++] = buf[r];
			r        = (r + 1) % N;
		}
		return n;
	}

	// Advances the read pointer by `count` samples (clamped to available).
	void skip(int count) {
		size_t r   = read_pos.load(std::memory_order_relaxed);
		size_t w   = write_pos.load(std::memory_order_acquire);
		int avail  = static_cast<int>((w - r + N) % N);
		int actual = std::min(count, avail);
		read_pos.store((r + actual) % N, std::memory_order_release);
	}

	int available() const {
		size_t w = write_pos.load(std::memory_order_acquire);
		size_t r = read_pos.load(std::memory_order_acquire);
		return static_cast<int>((w - r + N) % N);
	}

	void reset() {
		write_pos.store(0, std::memory_order_relaxed);
		read_pos.store(0, std::memory_order_release);
	}

private:
	std::array<float, N> buf {};
	std::atomic<size_t> write_pos {0};
	std::atomic<size_t> read_pos {0};
};