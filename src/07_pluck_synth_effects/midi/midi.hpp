#pragma once
#include "../voice/note_event.hpp"
#include "../voice/ring_buffer.hpp"
#include "synth_params.hpp"
#include <alsa/asoundlib.h>
#include <atomic>
#include <thread>
#include <unordered_map>

class MidiReader {
public:
	explicit MidiReader(RingBuffer<NoteEvent, 64> &event_queue, SynthParams &params);
	~MidiReader();

	bool open(std::initializer_list<const char *> device_names);
	void start();
	void stop();

private:
	void midi_loop();
	void handle_event(snd_seq_event_t *ev);
	int find_client(const char *search);

	static std::string note_name(int note);

	snd_seq_t *seq = nullptr;
	int in_port    = -1;
	std::thread thread;
	std::atomic<bool> running {false};

	RingBuffer<NoteEvent, 64> &event_queue;

	// MIDI CC Handling
	SynthParams &params;

	void handle_cc(int cc, int value);
};