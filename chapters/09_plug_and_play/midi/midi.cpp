#include "midi.hpp"
#include "../config.hpp"
#include <cmath>
#include <iostream>
#include <poll.h>
#include <string>
#include <vector>

MidiReader::MidiReader(RingBuffer<NoteEvent, 64> &event_queue, SynthParams &params) :
    event_queue(event_queue), params(params) {}

MidiReader::~MidiReader() { stop(); }

bool MidiReader::open(std::initializer_list<const char *> device_names) {
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		std::cerr << "MidiReader: failed to open sequencer\n";
		return false;
	}

	snd_seq_set_client_name(seq, "Midi Synth");

	in_port = snd_seq_create_simple_port(seq,
	                                     "MIDI In",
	                                     SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
	                                     SND_SEQ_PORT_TYPE_APPLICATION);
	if (in_port < 0) {
		std::cerr << "MidiReader: failed to create input port\n";
		return false;
	}

	// Subscribe to system announcements
	snd_seq_connect_from(seq, in_port, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);

	connect_all_inputs();

	return true;
}

void MidiReader::start() {
	running.store(true);
	if (on_port_change) on_port_change();
	thread = std::thread(&MidiReader::midi_loop, this);
}

void MidiReader::stop() {
	running.store(false);
	if (thread.joinable()) thread.join();
	if (seq) {
		snd_seq_close(seq);
		seq = nullptr;
	}
}

void MidiReader::midi_loop() {
	int npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
	std::vector<pollfd> pfds(npfds);
	snd_seq_poll_descriptors(seq, pfds.data(), npfds, POLLIN);

	while (running.load()) {
		int ret = poll(pfds.data(), npfds, 50);

		if (ret < 0) {
			if (errno == EINTR) continue;
			std::cerr << "MidiReader: poll error\n";
			break;
		}

		if (ret == 0) continue;

		while (snd_seq_event_input_pending(seq, 1) > 0) {
			snd_seq_event_t *ev;
			if (snd_seq_event_input(seq, &ev) < 0) break;
			handle_event(ev);
		}
	}
}

void MidiReader::handle_event(snd_seq_event_t *ev) {
	switch (ev->type) {
		case SND_SEQ_EVENT_NOTEON:
			if (ev->data.note.velocity > 0) {
				event_queue.push(
				    {NoteEvent::Type::NoteOn, ev->data.note.note, ev->data.note.velocity});
				std::cout << "Note On  | " << note_name(ev->data.note.note) << " ("
				          << (int)ev->data.note.note << ")"
				          << " vel=" << (int)ev->data.note.velocity << "\n";
			} else {
				// velocity 0 Note On is equivalent to Note Off
				event_queue.push({NoteEvent::Type::NoteOff, ev->data.note.note, 0});
				std::cout << "Note Off | " << note_name(ev->data.note.note) << "\n";
			}
			break;

		case SND_SEQ_EVENT_NOTEOFF:
			event_queue.push({NoteEvent::Type::NoteOff, ev->data.note.note, 0});
			std::cout << "Note Off | " << note_name(ev->data.note.note) << "\n";
			break;

		case SND_SEQ_EVENT_CONTROLLER: {
			int cc  = ev->data.control.param;
			int val = ev->data.control.value;
			params.handle_cc(cc, val);

			if (auto id = params.cc_to_param(cc)) {
				auto d = params.descriptor(*id);
				std::cout << "CC " << cc << " | " << d.name << " = " << params.get_value(*id) << " "
				          << d.unit << "\n";
				if (on_param_change)
					on_param_change(
					    *id, params.get_normalized(*id), params.get_value(*id), d.name, d.unit);
			} else {
				std::cout << "CC " << cc << " val=" << val << " (unmapped)\n";
			}
			break;
		}

		case SND_SEQ_EVENT_PORT_START:
			// A new port available
			std::cout << "MidiReader: New MIDI port detected. Re-scanning...\n";
			connect_all_inputs();
			if (on_port_change) on_port_change();
			break;

		case SND_SEQ_EVENT_CLIENT_EXIT:
			// A device was removed.
			// ALSA kills the connection automatically
			std::cout << "MidiReader: Device disconnected.\n";
			if (on_port_change) on_port_change();
			break;

		default: break;
	}
}

std::string MidiReader::note_name(int note) {
	static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	return std::string(names[note % 12]) + std::to_string((note / 12) - 1);
}

bool MidiReader::connect_all_inputs() {
	snd_seq_client_info_t *c_info;
	snd_seq_port_info_t *p_info;

	snd_seq_client_info_alloca(&c_info);
	snd_seq_port_info_alloca(&p_info);
	snd_seq_client_info_set_client(c_info, -1);

	int my_client_id = snd_seq_client_id(seq);

	while (snd_seq_query_next_client(seq, c_info) >= 0) {
		int client_id = snd_seq_client_info_get_client(c_info);

		// Skip itself to avoid infinite loops
		if (client_id == my_client_id) continue;

		// Skip System and Midi Through
		std::string name = snd_seq_client_info_get_name(c_info);
		if (name == "System" || name == "Midi Through") continue;

		// Look at all ports on this client
		snd_seq_port_info_set_client(p_info, client_id);
		snd_seq_port_info_set_port(p_info, -1);

		while (snd_seq_query_next_port(seq, p_info) >= 0) {
			unsigned int caps = snd_seq_port_info_get_capability(p_info);

			// Check if the port allows "READ" and "SUBS_READ"
			if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_SUBS_READ)) {
				int port_id = snd_seq_port_info_get_port(p_info);

				int err = snd_seq_connect_from(seq, in_port, client_id, port_id);

				if (err == 0) {
					std::cout << "Auto-connected: " << name << " [" << client_id << ":" << port_id
					          << "]\n";
				} else if (err == -EEXIST || err == -EBUSY) {
					// Already connected
				} else {
					std::cerr << "Failed to connect to " << name << ": " << snd_strerror(err)
					          << "\n";
				}
			}
		}
	}
	return true;
}

std::string MidiReader::get_connected_names() {
	if (!seq || in_port < 0) return "";

	std::string names = "";
	snd_seq_query_subscribe_t *subs;
	snd_seq_query_subscribe_alloca(&subs);

	snd_seq_query_subscribe_set_client(subs, snd_seq_client_id(seq));
	snd_seq_query_subscribe_set_port(subs, in_port);

	// Find who is writing to us
	snd_seq_query_subscribe_set_type(subs, SND_SEQ_QUERY_SUBS_WRITE);
	snd_seq_query_subscribe_set_index(subs, 0);

	while (snd_seq_query_port_subscribers(seq, subs) >= 0) {
		// This address represents the SENDER (the MIDI controller)
		const snd_seq_addr_t *addr = snd_seq_query_subscribe_get_addr(subs);

		snd_seq_client_info_t *c_info;
		snd_seq_client_info_alloca(&c_info);

		if (snd_seq_get_any_client_info(seq, addr->client, c_info) >= 0) {
			std::string client_name = snd_seq_client_info_get_name(c_info);

			if (client_name != "System" && client_name != "Midi Through") {
				if (names.find(client_name) == std::string::npos) {
					if (!names.empty()) names += ", ";
					names += client_name;
				}
			}
		}

		snd_seq_query_subscribe_set_index(subs, snd_seq_query_subscribe_get_index(subs) + 1);
	}

	return names.empty() ? "" : names;
}