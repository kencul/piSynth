#include "audio/audio.hpp"
#include "config.hpp"
#include "midi/midi.hpp"
#include "midi/synth_params.hpp"
#include "voice/note_event.hpp"
#include "voice/ring_buffer.hpp"
#include <atomic>
#include <csignal>
#include <iostream>

static std::atomic<bool> should_quit {false};

static void on_signal(int) { should_quit.store(true); }

int main() {
	RingBuffer<NoteEvent, 64> event_queue;
	SynthParams params;

	AudioEngine audio(event_queue, params);
	MidiReader midi(event_queue, params);

	if (!audio.open(Config::AUDIO_DEVICE)) return 1;
	if (!midi.open(Config::MIDI_DEVICES)) return 1;

	std::signal(SIGINT, on_signal);
	std::signal(SIGTERM, on_signal);

	audio.start();
	midi.start();

	std::cout << "Synth running. Press Ctrl+C to quit.\n";

	while (!should_quit.load()) pause();

	std::cout << "\nShutting down...\n";

	audio.stop();
	midi.stop();

	return 0;
}