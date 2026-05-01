#pragma once
#include "../config.hpp"
#include "../osc/osc.hpp"
#include <alsa/asoundlib.h>
#include <atomic>
#include <thread>

class AudioEngine {
public:
	AudioEngine(std::atomic<double> &frequency, std::atomic<unsigned int> &notes_active);
	~AudioEngine();

	bool open(const char *device);
	void start();
	void stop();

private:
	void audio_loop();
	bool configure_device();

	snd_pcm_t *handle = nullptr;
	Oscillator osc;
	std::thread thread;
	std::atomic<bool> running {false};

	std::atomic<double> &frequency;
	std::atomic<uint> &notes_active;

	// negotiated by configure_device, used by audio_loop
	snd_pcm_uframes_t period_size = Config::PERIOD_SIZE;
	snd_pcm_uframes_t buffer_size = Config::BUFFER_SIZE;
	unsigned int sample_rate      = Config::SAMPLE_RATE;
	unsigned int channels         = Config::CHANNELS;
};