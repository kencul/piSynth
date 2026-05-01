## 02 Midi Monitor

Takes MIDI input from a connected MIDI device and prints MIDI messages to the console.

### Usage

Run executable. Put MIDI device name as an argument:

```bash
/build/bin/02_midi_monitor device_name
```

To get the device name, use the following command:

```bash
aconnect -l
```

This will list all MIDI ports on the ALSA sequencer. Find the name of your desired MIDI controller and use it as an argument.

### MIDI Port Setup

This program uses the ALSA Sequencer API, which is an abstraction of the raw MIDI.

Each MIDI device acts as a port in the ALSA sequencer. The MIDI controller connected by USB is a port that sends MIDI. The script creates a receiving port and connects the two so the controller port sends its messages to the script.

First, a port is created:

```cpp
snd_seq_t *seq;
snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
```

This registeres a port to the ALSA sequencer with no name or connections.

Next, the port is named:

```cpp
snd_seq_set_client_name(seq, "MIDI Monitor");
```

The name of the port can be seen when using `aconnect`

```bash
aconnect -l
```

Next, the port is configured to take a input:

```cpp
int in_port = snd_seq_create_simple_port(
    seq, // port ref
    "Monitor In", // name of port
    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, // CAP_WRITE means port can receive messages, CAP_SUBS_WRITE makes it show up publically to other programs (shows up in aconnect -l)
    SND_SEQ_PORT_TYPE_APPLICATION // A description of what the port is for
);
```

Find the MIDI controller specified by the user:

```cpp
int src_client = find_client(seq, device_search);
```

`find_client()` then cycles through all available ports for one that contains the user provided string.

```cpp
static int find_client(snd_seq_t *seq, const char *search)
{
    snd_seq_client_info_t *info;
    snd_seq_client_info_alloca(&info);
    snd_seq_client_info_set_client(info, -1); // start of list sentinel

    while (snd_seq_query_next_client(seq, info) >= 0) { // advance one client per iteration
        const char *name = snd_seq_client_info_get_name(info);
        if (strstr(name, search)) // substring match
            return snd_seq_client_info_get_client(info);
    }
    return -1;
}
```

Finally, subscribe to the controller, connecting the controller port to the script port.

```cpp
snd_seq_connect_from(seq, in_port, src_client, 0);
```

Running `aconnect -l` should show this connection like this:

```bash
client 28: 'KOMPLETE KONTROL M32' [type=kernel,card=3]
    0 'KOMPLETE KONTROL M32 MIDI 1'
        Connecting To: 128:0
client 128: 'MIDI Monitor' [type=user,pid=4673]
    0 'Monitor In      '
        Connected From: 28:0
```

Now, a blocking loop can process incoming MIDI messages:

```cpp
snd_seq_event_t *ev;
while (snd_seq_event_input(seq, &ev) >= 0) {
```

`snd_seq_event_input()` puts the thread to sleep until an event arrives. the event arrives parsed by the kernel.

`ev->type` shows the MIDI message type. `ev->data` is a union where the active member depends on the MIDI type. Refer to the code for what member to use for what type.