// web/web_server.cpp
#include "web_server.hpp"
#include <iostream>

void WebServer::start(int port) {
	running.store(true);
	thread = std::thread(&WebServer::run, this, port);
}

void WebServer::stop() {
	if (!running.exchange(false)) return;

	// close the listen socket from within the uWS thread
	if (loop) {
		loop->defer([this] {
			if (listen_socket) {
				us_listen_socket_close(0, listen_socket);
				listen_socket = nullptr;
			}
		});
	}

	if (thread.joinable()) thread.join();
}

void WebServer::broadcast(std::string message) {
	if (!loop) return;

	// defer the send onto the uWS thread — the string is moved into the lambda
	loop->defer([this, msg = std::move(message)] {
		for (auto *ws : clients) ws->send(msg, uWS::OpCode::TEXT);
	});
}

void WebServer::run(int port) {
	// capture the loop pointer while we're on the uWS thread
	loop = uWS::Loop::get();

	uWS::App()
	    .get("/",
	         [](auto *res, auto * /*req*/) {
		         res->writeHeader("Content-Type", "text/html");
		         res->end(R"html(
                <!DOCTYPE html>
                <html>
                <head><title>Synth</title></head>
                <body>
                    <h1>Synth UI</h1>
                    <div id="status">Connecting...</div>
                    <div id="messages"></div>
                    <script>
                        const ws = new WebSocket(`ws://${location.host}/ws`);
                        const status = document.getElementById('status');
                        const messages = document.getElementById('messages');

                        ws.onopen = () => status.textContent = 'Connected';
                        ws.onclose = () => status.textContent = 'Disconnected';

                        ws.onmessage = (e) => {
                            const p = document.createElement('p');
                            p.textContent = e.data;
                            messages.prepend(p);
                        };

                        ws.onopen = () => {
                            status.textContent = 'Connected';
                            ws.send('hello from browser');
                        };
                    </script>
                </body>
                </html>
            )html");
	         })
	    .ws<PerSocketData>(
	        "/ws",
	        {.open =
	             [this](auto *ws) {
		             clients.insert(ws);
		             ws->send("hello from synth", uWS::OpCode::TEXT);
		             std::cout << "WebServer: client connected, total=" << clients.size() << "\n";
	             },
	         .message =
	             [this](auto *ws, std::string_view msg, uWS::OpCode) {
		             std::cout << "WebServer: received: " << msg << "\n";
		             // echo back
		             ws->send(msg, uWS::OpCode::TEXT);
	             },
	         .close =
	             [this](auto *ws, int /*code*/, std::string_view /*reason*/) {
		             clients.erase(ws);
		             std::cout << "WebServer: client disconnected, total=" << clients.size()
		                       << "\n";
	             }})
	    .listen(port,
	            [this, port](auto *token) {
		            listen_socket = token;
		            if (token) std::cout << "WebServer: listening on port " << port << "\n";
		            else
			            std::cerr << "WebServer: failed to bind port " << port << "\n";
	            })
	    .run(); // blocks until listen socket is closed

	std::cout << "WebServer: event loop exited\n";
}