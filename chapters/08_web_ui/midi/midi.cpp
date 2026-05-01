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

	bool any_connected = false;
	for (const char *name : device_names) {
		int client = find_client(name);
		if (client < 0) {
			std::cerr << "MidiReader: could not find device: " << name << "\n";
			continue;
		}
		snd_seq_connect_from(seq, in_port, client, 0);
		std::cout << "MidiReader: connected to " << name << " (client " << client << ":0)\n";
		any_connected = true;
	}

	return any_connected;
}

void MidiReader::start() {
	running.store(true);
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

		default: break;
	}
}

int MidiReader::find_client(const char *search) {
	snd_seq_client_info_t *info;
	snd_seq_client_info_alloca(&info);
	snd_seq_client_info_set_client(info, -1);

	while (snd_seq_query_next_client(seq, info) >= 0) {
		const char *name = snd_seq_client_info_get_name(info);
		if (strstr(name, search)) return snd_seq_client_info_get_client(info);
	}
	return -1;
}

std::string MidiReader::note_name(int note) {
	static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	return std::string(names[note % 12]) + std::to_string((note / 12) - 1);
}