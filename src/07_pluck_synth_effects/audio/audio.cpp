#include "audio.hpp"
#include <algorithm>
#include <iostream>
#include <pthread.h>
#include <vector>

AudioEngine::AudioEngine(RingBuffer<NoteEvent, 64> &event_queue, SynthParams &params) :
    event_queue(event_queue), params(params), voice_manager(params), master_bus(params) {}

AudioEngine::~AudioEngine() { stop(); }

bool AudioEngine::open(const char *device) {
	if (snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
		std::cerr << "AudioEngine: could not open device: " << device << "\n";
		return false;
	}

	if (!configure_device()) return false;

	voice_manager.init(period_size);
	master_bus.init(period_size);

	mix_l.assign(period_size, 0.0f);
	mix_r.assign(period_size, 0.0f);
	buf.assign(period_size * channels, 0);

	return true;
}

bool AudioEngine::configure_device() {
	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(handle, params);

	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(handle, params, channels);
	snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, nullptr);
	snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, nullptr);
	snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);

	int err;
	if ((err = snd_pcm_hw_params(handle, params)) < 0) {
		std::cerr << "AudioEngine: failed to apply hw params: " << snd_strerror(err) << "\n";
		return false;
	}

	std::cout << "AudioEngine: rate=" << sample_rate << " period=" << period_size
	          << " buffer=" << buffer_size << "\n";
	return true;
}

void AudioEngine::start() {
	running.store(true);
	thread = std::thread(&AudioEngine::audio_loop, this);

	// elevate above normal scheduler after thread is running
	sched_param sp {.sched_priority = 80};
	int err = pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &sp);
	if (err != 0)
		std::cerr << "AudioEngine: could not set realtime priority (missing cap_sys_nice?)\n";

	// read back what the kernel actually assigned
	int policy;
	pthread_getschedparam(thread.native_handle(), &policy, &sp);
	std::cout << "AudioEngine: policy=" << (policy == SCHED_FIFO ? "SCHED_FIFO" : "OTHER")
	          << " priority=" << sp.sched_priority << "\n";
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
	int xrun_count     = 0;
	auto session_start = std::chrono::steady_clock::now();

	while (running.load()) {
		// drain all pending MIDI events before generating audio
		while (auto ev = event_queue.pop()) voice_manager.handle(*ev);

		voice_manager.process(mix_l, mix_r);
		master_bus.process(mix_l, mix_r);

		for (int i = 0; i < static_cast<int>(period_size); ++i) {
			buf[i * channels + 0] =
			    static_cast<int16_t>(std::clamp(mix_l[i], -1.0f, 1.0f) * Config::SAMPLE_SCALE);
			buf[i * channels + 1] =
			    static_cast<int16_t>(std::clamp(mix_r[i], -1.0f, 1.0f) * Config::SAMPLE_SCALE);
		}

		snd_pcm_sframes_t written = snd_pcm_writei(handle, buf.data(), period_size);
		if (written < 0) {
			if (written == -EPIPE) {
				++xrun_count;
				auto elapsed   = std::chrono::steady_clock::now() - session_start;
				double minutes = std::chrono::duration<double>(elapsed).count() / 60.0;
				double rate    = minutes > 0.1 ? xrun_count / minutes : 0.0;
				std::cerr << "xrun #" << xrun_count << " (" << rate << "/min)\n";
			}

			written = snd_pcm_recover(handle, written, 0);
			if (written < 0) {
				std::cerr << "AudioEngine: unrecoverable error: " << snd_strerror(written) << "\n";
				break;
			}
		}
	}
}
