#pragma once
#include "../common/synth_params.hpp"
#include "../fft/fft_accumulator.hpp"
#include "../fft/fft_processor.hpp"
#include "messages.hpp"
#include "msg_dispatcher.hpp"
#include <App.h>
#include <atomic>
#include <string>
#include <thread>
#include <unordered_set>

struct PerSocketData {};
using WS = uWS::WebSocket<false, true, PerSocketData>;

class WebServer {
public:
	explicit WebServer(SynthParams &params, MsgDispatcher &dispatcher);
	void start(int port);
	void stop();
	void reset_fft();

	void set_fft_acc(FftAccumulator<Config::FFT_ACC_SIZE> *acc) { fft_acc = acc; }

	// Thread-safe: moves the struct into a defer lambda so the uWS thread serializes and sends.
	// Safe to call from the audio or MIDI thread.
	template <typename Msg> void broadcast(Msg msg) {
		if (!loop) return;
		loop->defer([this, m = std::move(msg)] { broadcast_direct(m.serialize()); });
	}

	void broadcast_midi_device(const std::string &device_names) {
		MIDIDeviceMsg msg {device_names};
		broadcast(msg);
		last_midi_device_msg = msg;
	}

	void broadcast_audio_device(const std::string &device_name) {
		AudioDeviceMsg msg {device_name};
		broadcast(msg);
		last_audio_device_msg = msg;
	}

private:
	void run(int port);
	void send_initial_state(WS *ws);

	// Only call from the uWS thread.
	void broadcast_direct(const std::string &msg);

	SynthParams &params;
	MsgDispatcher &dispatcher;
	std::string html;

	FftProcessor fft;
	us_timer_t *fft_timer                         = nullptr;
	FftAccumulator<Config::FFT_ACC_SIZE> *fft_acc = nullptr;

	std::thread thread;
	std::atomic<bool> running {false};

	std::unordered_set<WS *> clients;
	uWS::Loop *loop                   = nullptr;
	us_listen_socket_t *listen_socket = nullptr;

	MIDIDeviceMsg last_midi_device_msg;
	AudioDeviceMsg last_audio_device_msg;
};