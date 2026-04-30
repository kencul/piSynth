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

	auto broadcast_midi_device = [&web, &midi]() {
		std::cout << "Broadcasting MIDI device change: " << midi.get_connected_names() << "\n";
		web.broadcast_midi_device(midi.get_connected_names());
	};

	auto broadcast_audio_device = [&web, &audio]() {
		web.broadcast_audio_device(audio.get_device_name());
	};

	auto broadcast_presets = [&web, &params]() {
		auto list = params.get_preset_list();
		web.broadcast(PresetListMsg {list});
	};

	if (!midi.open()) return 1;

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

	midi.on_port_change   = broadcast_midi_device;
	audio.on_state_change = broadcast_audio_device;

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

	dispatcher.on("reset", [&params, &web](std::string_view /*msg*/) {
		params.reset_to_defaults();

		// Broadcast the new values to the UI so the knobs update visually
		for (int i = 0; i < static_cast<int>(SynthParams::ParamId::COUNT); ++i) {
			auto id = static_cast<SynthParams::ParamId>(i);
			auto d  = params.descriptor(id);
			web.broadcast(
			    ParamMsg {id, params.get_normalized(id), params.get_value(id), d.name, d.unit});
		}
	});

	dispatcher.on("load_preset", [&params, &web, &broadcast_presets](std::string_view msg) {
		std::string name = MsgParser::extract_string(msg, "name"); // See note below
		params.load_preset(name);
		for (int i = 0; i < static_cast<int>(SynthParams::ParamId::COUNT); ++i) {
			auto id = static_cast<SynthParams::ParamId>(i);
			auto d  = params.descriptor(id);
			web.broadcast(
			    ParamMsg {id, params.get_normalized(id), params.get_value(id), d.name, d.unit});
		}
	});

	dispatcher.on("save_preset", [&params, &broadcast_presets](std::string_view msg) {
		std::string name = MsgParser::extract_string(msg, "name");
		params.save_preset(name);
		broadcast_presets(); // Refresh UI list for everyone
	});

	dispatcher.on("delete_preset", [&params, &broadcast_presets](std::string_view msg) {
		std::string name = MsgParser::extract_string(msg, "name");
		if (!name.empty()) {
			params.delete_preset(name);
			broadcast_presets(); // Update the UI list
		}
	});

	std::signal(SIGINT, on_signal);
	std::signal(SIGTERM, on_signal);

	web.set_fft_acc(&audio.get_fft_acc());

	// audio.start();
	midi.start();
	web.start(Config::UI_PORT);

	std::cout << "Synth running. Press Ctrl+C to quit.\n";
	while (!should_quit.load()) {
		if (audio.open()) {
			web.reset_fft();
			audio.start();
			// Monitor the engine
			while (audio.is_running() && !should_quit.load()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			audio.stop();
		} else {
			// Wait and retry if no device is found
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
	std::cout << "\nShutting down...\n";

	audio.stop();
	midi.stop();
	web.stop();

	return 0;
}