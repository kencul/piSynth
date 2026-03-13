#include "audio.hpp"
#include <cmath>
#include <iostream>
#include <vector>

AudioEngine::AudioEngine(std::atomic<double> &frequency, std::atomic<unsigned int> &notes_active) :
    osc(sample_rate), frequency(frequency), notes_active(notes_active) {}

AudioEngine::~AudioEngine() { stop(); }

bool AudioEngine::open(const char *device) {
	if (snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
		std::cerr << "AudioEngine: could not open device: " << device << "\n";
		return false;
	}
	return configure_device();
}

bool AudioEngine::configure_device() {
	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(handle, params);

	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S32_LE);
	snd_pcm_hw_params_set_channels(handle, params, channels);

	snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, nullptr);

	snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, nullptr);
	snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);

	int err;
	if ((err = snd_pcm_hw_params(handle, params)) < 0) {
		std::cerr << "AudioEngine: failed to apply hw params: " << snd_strerror(err) << "\n";
		return false;
	}

	// update osc now that we know the negotiated rate
	osc = Oscillator(sample_rate);

	std::cout << "AudioEngine: rate=" << sample_rate << " period=" << period_size
	          << " buffer=" << buffer_size << "\n";
	return true;
}

void AudioEngine::start() {
	running.store(true);
	thread = std::thread(&AudioEngine::audio_loop, this);
}

void AudioEngine::stop() {
	running.store(false);
	if (thread.joinable()) thread.join();

	if (handle) {
		snd_pcm_drain(handle);
		snd_pcm_close(handle);
		handle = nullptr;
	}
}

void AudioEngine::audio_loop() {
	std::vector<int32_t> buf(period_size * channels);
	double current_freq = 0.0;

	while (running.load()) {
		double new_freq = frequency.load();

		// only update oscillator when frequency actually changes
		if (new_freq != current_freq) {
			osc.set_frequency(new_freq);
			current_freq = new_freq;
		}

		if (notes_active.load() > 0) osc.process(buf.data(), period_size, channels);
		else
			std::fill(buf.begin(), buf.end(), 0);

		snd_pcm_sframes_t written = snd_pcm_writei(handle, buf.data(), period_size);
		if (written < 0) {
			written = snd_pcm_recover(handle, written, 0);
			if (written < 0) {
				std::cerr << "AudioEngine: unrecoverable error: " << snd_strerror(written) << "\n";
				break;
			}
		}
	}
}