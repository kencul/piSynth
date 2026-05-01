#include "web_server.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

static std::string load_html(const std::string &path) {
	std::ifstream f(path);
	if (!f) {
		std::cerr << "WebServer: failed to open " << path << "\n";
		return "<html><body>index.html not found</body></html>";
	}
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

WebServer::WebServer(SynthParams &params, MsgDispatcher &dispatcher) :
    params(params), dispatcher(dispatcher) {
	html = load_html(HTML_PATH);
}

void WebServer::start(int port) {
	running.store(true);
	thread = std::thread(&WebServer::run, this, port);
}

void WebServer::stop() {
	if (!running.exchange(false)) return;

	if (fft_timer) {
		us_timer_close(fft_timer);
		fft_timer = nullptr;
	}
	fft.destroy();

	if (loop) {
		loop->defer([this] {
			auto clients_copy = clients;

			for (auto *ws : clients_copy) { ws->close(); }

			if (listen_socket) {
				us_listen_socket_close(0, listen_socket);
				listen_socket = nullptr;
			}
		});
	}

	if (thread.joinable()) thread.join();
}

void WebServer::broadcast_direct(const std::string &msg) {
	for (auto *ws : clients) {
		if (ws->getBufferedAmount() < 1024 * 1024) ws->send(msg, uWS::OpCode::TEXT);
		else
			std::cerr << "WebServer: dropping frame for slow client\n";
	}
}

void WebServer::send_initial_state(WS *ws) {
	ws->send(ConfigMsg {static_cast<int>(Config::SAMPLE_RATE), FftProcessor::OUT_BINS}.serialize(),
	         uWS::OpCode::TEXT);

	for (int i = 0; i < static_cast<int>(SynthParams::ParamId::COUNT); ++i) {
		auto id = static_cast<SynthParams::ParamId>(i);
		auto d  = params.descriptor(id);
		ws->send(ParamMsg {id, params.get_normalized(id), params.get_value(id), d.name, d.unit}
		             .serialize(),
		         uWS::OpCode::TEXT);
	}

	ws->send(last_midi_device_msg.serialize(), uWS::OpCode::TEXT);
	ws->send(last_audio_device_msg.serialize(), uWS::OpCode::TEXT);
	ws->send(PresetListMsg {params.get_preset_list()}.serialize(), uWS::OpCode::TEXT);
}

void WebServer::run(int port) {
	loop = uWS::Loop::get();

	fft.init();
	fft_timer = us_create_timer(reinterpret_cast<us_loop_t *>(loop), 0, sizeof(WebServer *));

	// store `this` in the timer's ext data
	*(WebServer **)us_timer_ext(fft_timer) = this;

	us_timer_set(
	    fft_timer,
	    [](us_timer_t *t) {
		    auto *self = *(WebServer **)us_timer_ext(t);
		    if (!self->fft_acc) return;
		    auto result = self->fft.process(*self->fft_acc);
		    if (result) self->broadcast_direct(result->serialize());
	    },
	    1000 / Config::UI_UPDATES_PER_SECOND,
	    1000 / Config::UI_UPDATES_PER_SECOND); // fire every 1000/UI_UPDATES_PER_SECOND ms, first
	                                           // fire after the same amount of time

	uWS::App()
	    .get("/",
	         [this](auto *res, auto *) {
		         res->cork([this, res] {
			         res->writeHeader("Content-Type", "text/html");
			         res->writeHeader("Cache-Control",
			                          "no-store"); // never cache — always fetch fresh
			         res->end(html);
		         });
	         })
	    .ws<PerSocketData>("/ws",
	                       {
	                           .compression      = uWS::DISABLED,
	                           .maxPayloadLength = 16 * 1024,
	                           .idleTimeout      = 30,
	                           .open =
	                               [this](auto *ws) {
		                               clients.insert(ws);
		                               send_initial_state(ws);
		                               std::cout << "WebServer: client connected ("
		                                         << clients.size() << " total)\n";
	                               },
	                           .message = [this](auto * /*ws*/,
	                                             std::string_view msg,
	                                             uWS::OpCode) { dispatcher.dispatch(msg); },
	                           .drain =
	                               [](auto *ws) {
		                               std::cout << "WebServer: buffer drained, remaining="
		                                         << ws->getBufferedAmount() << "\n";
	                               },
	                           .close =
	                               [this](auto *ws, int, std::string_view) {
		                               clients.erase(ws);
		                               std::cout << "WebServer: client disconnected ("
		                                         << clients.size() << " total)\n";
	                               },
	                       })
	    .listen(port,
	            [this, port](auto *token) {
		            listen_socket = token;
		            if (token) std::cout << "WebServer: listening on port " << port << "\n";
		            else
			            std::cerr << "WebServer: failed to bind port " << port << "\n";
	            })
	    .run();

	std::cout << "WebServer: event loop exited\n";
}

void WebServer::reset_fft() {
	if (fft_acc) { fft_acc->reset(); }
	fft.reset();
}