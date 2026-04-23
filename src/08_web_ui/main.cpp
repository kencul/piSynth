#include "audio/audio.hpp"
#include "common/synth_params.hpp"
#include "config.hpp"
#include "midi/midi.hpp"
#include "voice/note_event.hpp"
#include "voice/ring_buffer.hpp"
#include "web/messages.hpp"
#include "web/msg_dispatcher.hpp"
#include "web/msg_parser.hpp"
#include "web/web_server.hpp"
#include <atomic>
#include <csignal>
#include <iostream>

static std::atomic<bool> should_quit {false};
static void on_signal(int) { should_quit.store(true); }

int main() {
	RingBuffer<NoteEvent, 64> event_queue;
	SynthParams params;
	MsgDispatcher dispatcher;

	AudioEngine audio(event_queue, params);
	MidiReader midi(event_queue, params);
	WebServer web(params, dispatcher);

	if (!audio.open(Config::AUDIO_DEVICE)) return 1;
	if (!midi.open(Config::MIDI_DEVICES)) return 1;

	// audio thread -> web: meter data at ~30fps
	audio.on_meter = [&web](float rl, float rr, float pl, float pr) {
		web.broadcast(MeterMsg {rl, rr, pl, pr});
	};

	// audio thread -> web: waveguide state at ~30fps
	audio.on_waveguide = [&web](WaveguideSnapshot snap) {
		web.broadcast(WaveguideMsg {std::move(snap)});
	};

	// MIDI thread -> web: broadcast param change to all clients
	midi.on_param_change =
	    [&web](SynthParams::ParamId id, float norm, float val, const char *name, const char *unit) {
		    web.broadcast(ParamMsg {id, norm, val, name, unit});
	    };

	// web thread -> synth: apply slider change, echo to all clients for multi-client sync
	dispatcher.on("set_param", [&params, &web](std::string_view msg) {
		int id_int  = MsgParser::extract_int(msg, "id");
		float value = MsgParser::extract_float(msg, "value");

		if (id_int < 0 || id_int >= static_cast<int>(SynthParams::ParamId::COUNT)) return;

		auto id = static_cast<SynthParams::ParamId>(id_int);
		params.set_param(id, value);

		auto d = params.descriptor(id);
		web.broadcast(ParamMsg {id, value, params.get_value(id), d.name, d.unit});
	});

	std::signal(SIGINT, on_signal);
	std::signal(SIGTERM, on_signal);

	web.set_fft_acc(&audio.get_fft_acc());

	audio.start();
	midi.start();
	web.start(9002);

	std::cout << "Synth running. Press Ctrl+C to quit.\n";
	while (!should_quit.load()) pause();
	std::cout << "\nShutting down...\n";

	audio.stop();
	midi.stop();
	web.stop();

	return 0;
}