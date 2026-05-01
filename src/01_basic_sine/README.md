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