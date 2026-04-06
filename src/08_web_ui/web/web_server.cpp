#include "web_server.hpp"
#include <iostream>

static constexpr const char *HTML = R"html(
<!DOCTYPE html>
<html>
<head>
<title>Synth</title>
<style>
  body { font-family: monospace; background: #111; color: #ccc; padding: 20px; margin: 0; }
  #status { margin-bottom: 16px; color: #888; }
  .param { display: flex; align-items: center; gap: 8px; margin: 3px 0; }
  .param label { width: 160px; font-size: 12px; }
  .param input  { flex: 1; }
  .param .val   { width: 90px; text-align: right; font-size: 12px; }
</style>
</head>
<body>
<div id="status">Connecting...</div>
<div id="params"></div>
<script>
  const ws = new WebSocket(`ws://${location.host}/ws`);
  const status    = document.getElementById('status');
  const paramsDiv = document.getElementById('params');
  const sliders   = {};

  ws.onopen  = () => (status.textContent = 'Connected');
  ws.onclose = () => (status.textContent = 'Disconnected');

  ws.onmessage = ({ data }) => {
    const msg = JSON.parse(data);

    if (msg.type === 'param') {
      if (sliders[msg.id]) {
        sliders[msg.id].slider.value       = msg.normalized;
        sliders[msg.id].val.textContent    = `${msg.value.toFixed(2)} ${msg.unit}`;
      } else {
        const row = document.createElement('div');
        row.className = 'param';
        row.innerHTML =
          `<label>${msg.name}</label>` +
          `<input type="range" min="0" max="1" step="0.001" value="${msg.normalized}">` +
          `<span class="val">${msg.value.toFixed(2)} ${msg.unit}</span>`;

        const slider = row.querySelector('input');
        const val    = row.querySelector('.val');
        sliders[msg.id] = { slider, val };

        // type must be first key -- required by MsgDispatcher::extract_type
        slider.addEventListener('input', () =>
          ws.send(JSON.stringify({ type: 'set_param', id: msg.id, value: +slider.value }))
        );

        paramsDiv.appendChild(row);
      }
    }
  };
</script>
</body>
</html>
)html";

WebServer::WebServer(SynthParams &params, MsgDispatcher &dispatcher) :
    params(params), dispatcher(dispatcher) {}

void WebServer::start(int port) {
	running.store(true);
	thread = std::thread(&WebServer::run, this, port);
}

void WebServer::stop() {
	if (!running.exchange(false)) return;

	if (loop) {
		loop->defer([this] {
			for (auto *ws : clients) ws->close();
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
	for (int i = 0; i < static_cast<int>(SynthParams::ParamId::COUNT); ++i) {
		auto id = static_cast<SynthParams::ParamId>(i);
		auto d  = params.descriptor(id);
		ws->send(ParamMsg {id, params.get_normalized(id), params.get_value(id), d.name, d.unit}
		             .serialize(),
		         uWS::OpCode::TEXT);
	}
}

void WebServer::run(int port) {
	loop = uWS::Loop::get();

	uWS::App()
	    .get("/",
	         [](auto *res, auto *) {
		         res->cork([res] {
			         res->writeHeader("Content-Type", "text/html");
			         res->end(HTML);
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