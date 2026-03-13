#include <alsa/asoundlib.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <unistd.h>

// S32_LE samples are 32-bit containers with 24-bit precision
static const int32_t AMPLITUDE = 0x7FFFFF * 256; // scale 24-bit value to fill 32-bit container

static const unsigned int SAMPLE_RATE  = 44100;
static const unsigned int CHANNELS     = 2;
static const unsigned int PERIOD_SIZE  = 64;   // frames per period (~5.8ms at 44100Hz)
static const unsigned int BUFFER_SIZE  = PERIOD_SIZE * 4;

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

int main(int argc, char **argv)
{
    double frequency   = 440.0;
    int    duration_sec = 2;

    int opt;
    while ((opt = getopt(argc, argv, "f:d:")) != -1) {
        switch (opt) {
            case 'f': frequency    = std::stod(optarg); break;
            case 'd': duration_sec = std::stoi(optarg); break;
        }
    }

    snd_pcm_t *handle;
    if (snd_pcm_open(&handle, "hw:UR22mkII", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        std::cerr << "Could not open audio device.\n";
        return 1;
    }

    if (!configure_device(handle)) {
        snd_pcm_close(handle);
        return 1;
    }

    std::vector<int32_t> period_buf(PERIOD_SIZE * CHANNELS);

    double phase     = 0.0;
    double phase_inc = (2.0 * M_PI * frequency) / SAMPLE_RATE;

    long total_frames   = static_cast<long>(SAMPLE_RATE) * duration_sec;
    long frames_written = 0;

    std::cout << "Playing " << frequency << "Hz for " << duration_sec << "s...\n";

    while (frames_written < total_frames) {
        long frames_remaining = total_frames - frames_written;
        long frames_this_period = std::min(frames_remaining, static_cast<long>(PERIOD_SIZE));

        for (long i = 0; i < frames_this_period; ++i) {
            int32_t sample = static_cast<int32_t>(AMPLITUDE * std::sin(phase));
            period_buf[i * 2]     = sample; // left
            period_buf[i * 2 + 1] = sample; // right
            phase += phase_inc;

            if (phase >= 2.0 * M_PI)
                phase -= 2.0 * M_PI;
        }

        snd_pcm_sframes_t written = snd_pcm_writei(handle, period_buf.data(), frames_this_period);

        if (written < 0) {
            // snd_pcm_recover handles both xruns (EPIPE) and suspends (ESTRPIPE)
            written = snd_pcm_recover(handle, written, 0);
            if (written < 0) {
                std::cerr << "Unrecoverable write error: " << snd_strerror(written) << "\n";
                break;
            }
        }

        frames_written += written;
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    return 0;
}