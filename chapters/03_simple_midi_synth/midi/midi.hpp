#pragma once
#include <alsa/asoundlib.h>
#include <atomic>
#include <thread>

class MidiReader {
public:
	MidiReader(std::atomic<double> &frequency, std::atomic<unsigned int> &notes_active);
	~MidiReader();

	bool open(const char *device_name);
	void start();
	void stop();

private:
	void midi_loop();
	int find_client(const char *search);
	void handle_event(snd_seq_event_t *ev);

	// converts MIDI note number to Hz
	static double midi_to_hz(int note);
	// converts MIDI note number to human readable name for logging
	static std::string note_name(int note);

	snd_seq_t *seq = nullptr;
	int in_port    = -1;
	std::thread thread;
	std::atomic<bool> running {false};

	std::atomic<double> &frequency;
	std::atomic<unsigned int> &notes_active;
};