#pragma once
#include "../common/synth_params.hpp"
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

	// Thread-safe: moves the struct into a defer lambda so the uWS thread serializes and sends.
	// Safe to call from the audio or MIDI thread.
	template <typename Msg> void broadcast(Msg msg) {
		if (!loop) return;
		loop->defer([this, m = std::move(msg)] { broadcast_direct(m.serialize()); });
	}

private:
	void run(int port);
	void send_initial_state(WS *ws);

	// Only call from the uWS thread.
	void broadcast_direct(const std::string &msg);

	SynthParams &params;
	MsgDispatcher &dispatcher;

	std::thread thread;
	std::atomic<bool> running {false};

	std::unordered_set<WS *> clients;
	uWS::Loop *loop                   = nullptr;
	us_listen_socket_t *listen_socket = nullptr;
};