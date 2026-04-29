#pragma once
#include "../common/synth_params.hpp"
#include "../voice/note_event.hpp"
#include "../voice/ring_buffer.hpp"
#include <alsa/asoundlib.h>
#include <atomic>
#include <functional>
#include <thread>
#include <unordered_map>

class MidiReader {
public:
	explicit MidiReader(RingBuffer<NoteEvent, 64> &event_queue, SynthParams &params);
	~MidiReader();

	bool open(std::initializer_list<const char *> device_names = {});
	void start();
	void stop();

	std::string get_connected_names();

	std::function<void(
	    SynthParams::ParamId, float normalized, float value, const char *name, const char *unit)>
	    on_param_change;

	std::function<void()> on_port_change;

private:
	void midi_loop();
	void handle_event(snd_seq_event_t *ev);
	bool connect_all_inputs();

	static std::string note_name(int note);

	snd_seq_t *seq = nullptr;
	int in_port    = -1;
	std::thread thread;
	std::atomic<bool> running {false};

	RingBuffer<NoteEvent, 64> &event_queue;

	// MIDI CC Handling
	SynthParams &params;
};