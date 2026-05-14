#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

#include "voice/note_event.hpp"
#include "voice/ring_buffer.hpp"

// ── helpers ──

static NoteEvent make_note(int n) { return {NoteEvent::Type::NoteOn, n, 64}; }

// ── single-threaded correctness ──

TEST_CASE("RingBuffer pop on empty returns nullopt", "[ring_buffer]") {
	RingBuffer<NoteEvent, 8> rb;
	CHECK_FALSE(rb.pop().has_value());
	CHECK(rb.empty());
}

TEST_CASE("RingBuffer preserves FIFO order", "[ring_buffer]") {
	RingBuffer<NoteEvent, 8> rb;

	for (int i = 0; i < 5; ++i) REQUIRE(rb.push(make_note(i)));

	for (int i = 0; i < 5; ++i) {
		auto item = rb.pop();
		REQUIRE(item.has_value());
		CHECK(item->note == i);
	}
	CHECK(rb.empty());
}

// CAPACITY=8 means 7 usable slots (one slot is sacrificed to distinguish full from empty).
TEST_CASE("RingBuffer full at CAPACITY-1 items", "[ring_buffer]") {
	RingBuffer<NoteEvent, 8> rb;

	for (int i = 0; i < 7; ++i) REQUIRE(rb.push(make_note(i)));

	// 8th push must be rejected — buffer is full
	CHECK_FALSE(rb.push(make_note(99)));
}

TEST_CASE("RingBuffer can be refilled after draining", "[ring_buffer]") {
	RingBuffer<NoteEvent, 8> rb;

	for (int i = 0; i < 7; ++i) rb.push(make_note(i));
	for (int i = 0; i < 7; ++i) rb.pop();

	REQUIRE(rb.empty());

	// Should accept a full batch again with no stale state
	for (int i = 10; i < 17; ++i) REQUIRE(rb.push(make_note(i)));

	for (int i = 10; i < 17; ++i) CHECK(rb.pop()->note == i);
}

// ── concurrent correctness ──

// One producer pushes N note-on events; one consumer pops them. After joining, every event must
// have arrived in order with no duplicates and no drops.
//
// This validates the acquire/release ordering that the class relies on to be safe under ARM's weak
// memory model. If the atomic operations were wrong, a concurrent run would occasionally produce
// stale reads or lost writes.
TEST_CASE("RingBuffer SPSC: all events arrive in order under concurrent access",
          "[ring_buffer][concurrent]") {
	constexpr int N      = 10'000;
	constexpr size_t CAP = 64;

	RingBuffer<NoteEvent, CAP> rb;
	std::vector<NoteEvent> received;
	received.reserve(N);

	std::atomic<bool> done {false};

	std::thread producer([&] {
		for (int i = 0; i < N; ++i) {
			// Spin until there is space — models how the MIDI thread retries
			while (!rb.push(make_note(i))) {}
		}
		done.store(true, std::memory_order_release);
	});

	std::thread consumer([&] {
		while (!done.load(std::memory_order_acquire) || !rb.empty()) {
			if (auto ev = rb.pop()) received.push_back(*ev);
		}
	});

	producer.join();
	consumer.join();

	// All N events must have arrived
	REQUIRE(received.size() == N);

	// They must be in the exact order they were pushed
	for (int i = 0; i < N; ++i) CHECK(received[i].note == i);
}
