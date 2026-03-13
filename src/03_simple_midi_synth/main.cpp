#include "audio/audio.hpp"
#include "config.hpp"
#include "midi/midi.hpp"
#include "osc/osc.hpp"
#include <atomic>
#include <csignal>
#include <iostream>

// signal flag lets the main thread exit cleanly on Ctrl+C
static std::atomic<bool> should_quit {false};

static void on_signal(int) { should_quit.store(true); }

int main() {
	// shared state — owned here, passed by reference to engine and reader
	std::atomic<double> frequency {0.0};
	std::atomic<unsigned int> notes_active {0};

	AudioEngine audio(frequency, notes_active);
	MidiReader midi(frequency, notes_active);

	if (!audio.open(Config::AUDIO_DEVICE)) return 1;

	if (!midi.open(Config::MIDI_DEVICE)) return 1;

	std::signal(SIGINT, on_signal);  // Ctrl+C
	std::signal(SIGTERM, on_signal); // kill

	audio.start();
	midi.start();

	std::cout << "Synth running. Press Ctrl+C to quit.\n";

	// block main thread until signal arrives
	while (!should_quit.load()) pause();

	std::cout << "\nShutting down...\n";

	audio.stop();
	midi.stop();

	return 0;
}