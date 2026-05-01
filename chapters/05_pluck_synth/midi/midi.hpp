#pragma once
#include <alsa/asoundlib.h>
#include <atomic>
#include <thread>
#include "../voice/note_event.hpp"
#include "../voice/ring_buffer.hpp"

class MidiReader {
public:
    explicit MidiReader(RingBuffer<NoteEvent, 64> &event_queue);
    ~MidiReader();

    bool open(const char *device_name);
    void start();
    void stop();

private:
    void midi_loop();
    void handle_event(snd_seq_event_t *ev);
    int  find_client(const char *search);

    static std::string note_name(int note);

    snd_seq_t                  *seq      = nullptr;
    int                         in_port  = -1;
    std::thread                 thread;
    std::atomic<bool>           running  { false };

    RingBuffer<NoteEvent, 64>  &event_queue;
};