#include <alsa/asoundlib.h>
#include <iostream>

// Converts MIDI note number to human-readable name (e.g. 60 -> "C4")
static std::string note_name(int note)
{
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return std::string(names[note % 12]) + std::to_string((note / 12) - 1);
}

// Finds a sequencer client by partial name match. Returns client id or -1.
static int find_client(snd_seq_t *seq, const char *search)
{
    snd_seq_client_info_t *info;
    snd_seq_client_info_alloca(&info);
    snd_seq_client_info_set_client(info, -1);

    while (snd_seq_query_next_client(seq, info) >= 0) {
        const char *name = snd_seq_client_info_get_name(info);
        if (strstr(name, search))
            return snd_seq_client_info_get_client(info);
    }
    return -1;
}

int main(int argc, char **argv)
{
    const char *device_search = (argc > 1) ? argv[1] : "KOMPLETE KONTROL";

    // Open the sequencer in duplex mode to read events
    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        std::cerr << "Failed to open sequencer\n";
        return 1;
    }

    snd_seq_set_client_name(seq, "MIDI Monitor");

    // Create an input port for receiving events
    int in_port = snd_seq_create_simple_port(
        seq, "Monitor In",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION
    );
    if (in_port < 0) {
        std::cerr << "Failed to create input port\n";
        return 1;
    }

    // Find and subscribe to the MIDI controller
    int src_client = find_client(seq, device_search);
    if (src_client < 0) {
        std::cerr << "Could not find device matching: " << device_search << "\n";
        return 1;
    }

    // Subscribe: route events from src_client:0 into input port
    snd_seq_connect_from(seq, in_port, src_client, 0);

    // Print assigned client id so user can see routing
    std::cout << "Monitoring: " << device_search
              << " (client " << src_client << ":0)\n"
              << "Client ID: " << snd_seq_client_id(seq) << ":" << in_port << "\n"
              << "Waiting for MIDI...\n\n";

    // Event loop: snd_seq_event_input blocks until an event arrives
    snd_seq_event_t *ev;
    while (snd_seq_event_input(seq, &ev) >= 0) {
        switch (ev->type) {
            case SND_SEQ_EVENT_NOTEON:
                if (ev->data.note.velocity > 0) {
                    std::cout << "Note On  | ch=" << (int)ev->data.note.channel
                              << " note=" << note_name(ev->data.note.note)
                              << " (" << (int)ev->data.note.note << ")"
                              << " vel=" << (int)ev->data.note.velocity << "\n";
                } else {
                    // velocity=0 Note On is the MIDI spec's alternative to Note Off
                    std::cout << "Note Off | ch=" << (int)ev->data.note.channel
                              << " note=" << note_name(ev->data.note.note) << "\n";
                }
                break;

            case SND_SEQ_EVENT_NOTEOFF:
                std::cout << "Note Off | ch=" << (int)ev->data.note.channel
                          << " note=" << note_name(ev->data.note.note) << "\n";
                break;

            case SND_SEQ_EVENT_CONTROLLER:
                std::cout << "CC       | ch=" << (int)ev->data.control.channel
                          << " cc="    << (int)ev->data.control.param
                          << " val="   << (int)ev->data.control.value << "\n";
                break;

            case SND_SEQ_EVENT_PITCHBEND:
                std::cout << "Pitchbend| ch=" << (int)ev->data.control.channel
                          << " val="   << (int)ev->data.control.value << "\n";
                break;

            default:
                // Ignore other types
                break;
        }
    }

    snd_seq_close(seq);
    return 0;
}