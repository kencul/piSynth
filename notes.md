# Raspberry Pi Notes
## Ken Kobayashi

---

## Init

Download [Raspberry Pi Imager](https://www.raspberrypi.com/software/) and set up SD card with Raspberry Pi OS.

I created an image with the Raspberry Pi OS Lite, as I won't be using the display out. This is to maximize performance for the audio processing.

When creating the OS, set up SSH public key.

``` powershell
ssh-keygen -t ed25519 -C "pi5"
```

Creates a ssh key pair. Enter the public key's (`.pub` file) content into the Pi image when prompted in the Imager.

Power up Pi with SD card with image, then SSH into it:

```powershell
ssh user_name@device_name.local
```

for me:

```powershell
ssh ken@ken-pi.local
```

in the ssh directory, make the file `config`, and add the entry for the login:

```
Host myPi
    HostName device_name.local
    User user_name
```

This lets you ssh into the pi with the shorthand:

```
ssh myPi
```

Check and install updates:

```bash
sudo apt update && sudo apt full-upgrade -y
```

Install tmux on the pi for better SSH experience:

```bash
sudo apt install tmux
```

Create a config file for tmux:

```bash
nano ~/.tmux.conf
```

Add these lines to the file:

```
set -g mouse on
set -as terminal-features ",xterm-256color:clipboard"
set -s escape-time 50
```

The first line lets you adjust terminal borders with the mouse. The last two fix a handshake issue with tmux on the pi and windows terminal.


## Testing Audio Output

I connected my USB audio interface to the Pi. To ensure this works and to get familiar with the interface, I will copy a sound file over SSH to the Pi and play it.

Front a local terminal, send files as follows:

```bash
scp file.wav user@pi:~/destinationfile.wav
```

In the SSH session, check audio device:

```bash
aplay -l
```

Look for the USB audio device, and note the card number and device number.

Play the file (if audio device card 0 and number 0):

```bash
aplay -D plughw:0,0 ~/test.wav
```

You can adjust the volume for audio devices with controls with `alsamixer`.

```bash
alsamixer
```

## Dev Environment

As I want to work on my windows PC for programming without dealing with cross-compilation, I set up remote SSH in VSCode.

First install the Remote Development extension. Press F1, and choose `Remote-SSH: Connect to Host...`. Select the pi ssh login you saved earlier. 
VSCode now opens in the Pi's filesystem, with the VSCode terminal already SSH'ed in. All changes to the files are done in the Pi, and building and running is done on the Pi.

To finish, install `cmake` and ALSA headers on the Pi to be able to compile code for audio:

```bash
sudo apt install build-essential gdb cmake libasound2-dev
```

This is possible because the Pi 5 I'm using is powerful enough to host a VSCode server and compile on its own. This is not possible when using a weaker board.

## Project setup

Make a `CMakeList.txt` to set up the project.

Create a `main.cpp` file with test code:

```cpp
#include <stdio.h>

int main() {
    printf("Hello, World!\n");
    return 0;
}
```

Make build dir:

```bash
mkdir -p build && cd build
cmake ..
```

Install `ninja` to build the project for fast builds:

```bash
sudo apt update
sudo apt install ninja
```

Run `ninja` in build directory

```bash
ninja
```

Run test executable:

```bash
./synth
```

In the future, the executable itself should be run in a separate terminal running tmux so it stays alive on the Pi.

## Git

Install git:

```bash
sudo apt update
sudo apt install git
```

set git configs. email should match github account:

```bash
git config --global user.email "you@example.com"
git config --global user.name "Your Name"
```

Create SSH key:

```bash
ssh-keygen -t ed25519 -C "kenkoba02@icloud.com"
```

Register public key in Github settings under `SSH and GPG keys`.

Prepare git repo. Ensure the link to the repo is the one for SSH:

```bash
git init
git add -A
git commit -m "init"
git remote add origin git@github.com:USERNAME/REPOSITORY.git
git branch -M main
git push -u origin main
```

## 01 Basic Sine

A basic sine generator that takes freq and duration as flag arguments and outputs real-time audio.

### Usage

Ensure audio configs are set to your audio device.

Change the device name for `snd_pcm_open()`, change `AMPLITUDE`, `SAMPLE_RATE`, `PERIOD_SIZE` and `CHANNELS` to match device specifications, and the data formats in `configure_device()`.

Build files:

```bash
mkdir -p build && cd build
cmake ..
ninja
```

All executables go into `/build/bin`.

Run `/build/bin/01_basic_sine -f 440 -d 2` to hear a 440hz sine wave for 2 seconds.

### Audio Device Config

The audio output device should be set up properly before this is done, as I have hardcoded my USB interface with the proper settings to make ALSA use my interface properly.

Run `aplay -l` to see the card number and device number and device name of the device you want to use.

In `/src/01_basic_sine`, look at line 62:

```cpp
if (snd_pcm_open(&handle, "hw:UR22mkII", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
```

change "hw:UR22mkII" to be the card and device number of your audio device ("hw:1,0" for card 1 device 0) or the name of your audio device.

Next, you must determine the format the audio device needs. Run the following command, changing the card number to your device's number:

```bash
cat /proc/asound/card0/stream0
```

This outputs this for my interface:

```bash
ken@ken-pi:~/synth/build $ cat /proc/asound/card0/stream0
Yamaha Corporation Steinberg UR22mkII at usb-xhci-hcd.1-2.1, high speed : USB Audio

Playback:
  Status: Stop
  Interface 1
    Altset 1
    Format: S32_LE
    Channels: 2
    Endpoint: 0x03 (3 OUT) (ASYNC)
    Rates: 44100, 48000, 88200, 96000, 176400, 192000
    Data packet interval: 125 us
    Bits: 24
    Channel map: FL FR
    Sync Endpoint: 0x83 (3 IN)
    Sync EP Interface: 1
    Sync EP Altset: 1
    Implicit Feedback Mode: No

Capture:
  Status: Stop
  Interface 2
    Altset 1
    Format: S32_LE
    Channels: 2
    Endpoint: 0x81 (1 IN) (ASYNC)
    Rates: 44100, 48000, 88200, 96000, 176400, 192000
    Data packet interval: 125 us
    Bits: 24
    Channel map: FL FR
```

What is important here is the `Playback` section. It tells me that I need to use `S32_LE` format, which is 32 bit little endian. However, the `Bits` section tells me the interface uses 24 bit depth, so 24 out of the 32 bits of data are actually processed, with the top 8 bits used as padding. The `Rates` section tells me the valid sample rates the interface handles. `Channels` tells me how many channels of audio I must output, in this case 2 channels or stereo.

This data is set in the source file near the top:

```cpp
// S32_LE samples are 32-bit containers with 24-bit precision
static const int32_t AMPLITUDE = 0x7FFFFF * 256; // scale 24-bit value to fill 32-bit container

static const unsigned int SAMPLE_RATE  = 44100;
static const unsigned int CHANNELS     = 2;
static const unsigned int PERIOD_SIZE  = 64;   // frames per period (~5.8ms at 44100Hz)
static const unsigned int BUFFER_SIZE  = PERIOD_SIZE * 4;
```

This data is then used in the `configure_device()` function:

```cpp
// Opens and configures the PCM device. Returns false on failure.
static bool configure_device(snd_pcm_t *handle)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params); // fill with full config space

    // Lock in format choices
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S32_LE);
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);

    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, nullptr);

    // Period and buffer sizes
    snd_pcm_uframes_t period = PERIOD_SIZE;
    snd_pcm_uframes_t buffer = BUFFER_SIZE;
    snd_pcm_hw_params_set_period_size_near(handle, params, &period, nullptr);
    snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer);

    int err;
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        std::cerr << "Failed to apply hw params: " << snd_strerror(err) << "\n";
        return false;
    }

    std::cout << "Device configured: rate=" << rate
              << " period=" << period << " buffer=" << buffer << "\n";
    return true;
}
```

The first 3 lines set up the opaque struct provided by ALSA for configuration. This part is something ALSA encapsulates away that I don't need to worry too much about.

Next 3 lines set the data format. I set it to interleaved 32 bit little endian with 2 channels as my interface requires.

Next, the sample rate is set to 44.1kHz. the `near` function for this requests the nearest sample rate that is compatible, just in case. The 4th argument indicates which way to round: -1 to round down, nullptr for nearest, 1 to round up. The chosen sample rate is saved back into rate to be read.

Finally, the period and buffer sizes are chosen. The period is how large one audio chunk is, and the buffer size is the total size of the circular buffer. The lower period size means less latency but more CPU stress. 64 is a comfortable level for synths.

The rest is mostly standard DSP code for a simple sine wave.

The buffer is sent to ALSA drivers to be process as such:

```cpp
snd_pcm_sframes_t written = snd_pcm_writei(handle, period_buf.data(), frames_this_period);
```

the `i` stands for interleaved.

There is a check to see if the write was successful:

```cpp
if (written < 0) {
    // snd_pcm_recover handles both xruns (EPIPE) and suspends (ESTRPIPE)
    written = snd_pcm_recover(handle, written, 0);
    if (written < 0) {
        std::cerr << "Unrecoverable write error: " << snd_strerror(written) << "\n";
        break;
    }
}
```

if an xrun or suspend happens, the stream must be recovered to continue playing. An xrun is when the buffer runs empty so garbage was played. A suspend is when the hardware turns off mid stream, such as going to sleep.

`snd_pcm_recover()` handles both cases so writes can continue from the next period.

And finally clean up at the end of the program:

```cpp
snd_pcm_drain(handle);
snd_pcm_close(handle);
```

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

