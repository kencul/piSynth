// web/web_server.hpp
#pragma once
#include <App.h> // uWebSockets
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

struct PerSocketData {};

class WebServer {
public:
	void start(int port);
	void stop();

	// thread-safe: callable from any thread
	void broadcast(std::string message);

private:
	void run(int port);

	std::thread thread;
	std::atomic<bool> running {false};

	// owned by the uWS thread only
	std::unordered_set<uWS::WebSocket<false, true, PerSocketData> *> clients;

	// used to schedule work onto the uWS thread from outside
	uWS::Loop *loop                   = nullptr;
	us_listen_socket_t *listen_socket = nullptr;
};