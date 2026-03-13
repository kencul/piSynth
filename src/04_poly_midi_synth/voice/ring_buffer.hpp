#pragma once
#include <array>
#include <atomic>
#include <optional>

// Single-producer single-consumer lock-free ring buffer.
// Safe only when exactly one thread calls push() and one thread calls pop().
template <typename T, size_t CAPACITY> class RingBuffer {
public:
	// Returns false if the buffer is full and the event was dropped
	bool push(const T &item) {
		size_t write = write_pos.load(std::memory_order_relaxed);
		size_t next  = (write + 1) % CAPACITY;

		// buffer full
		if (next == read_pos.load(std::memory_order_acquire)) return false;

		data[write] = item;

		// publish the write: must happen after data is written
		write_pos.store(next, std::memory_order_release);
		return true;
	}

	// Returns the item if one is available, std::nullopt if empty
	std::optional<T> pop() {
		size_t read = read_pos.load(std::memory_order_relaxed);

		// empty — read head has caught up to write head
		if (read == write_pos.load(std::memory_order_acquire)) return std::nullopt;

		T item = data[read];

		// publish the read: must happen after data is copied
		read_pos.store((read + 1) % CAPACITY, std::memory_order_release);
		return item;
	}

	bool empty() const {
		return read_pos.load(std::memory_order_acquire)
		    == write_pos.load(std::memory_order_acquire);
	}

private:
	std::array<T, CAPACITY> data;
	std::atomic<size_t> write_pos {0};
	std::atomic<size_t> read_pos {0};
};