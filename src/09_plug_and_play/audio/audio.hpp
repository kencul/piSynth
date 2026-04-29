#pragma once
#include "../common/synth_params.hpp"
#include "../common/waveguide_snapshot.hpp"
#include "../config.hpp"
#include "../effects/master_bus.hpp"
#include "../fft/fft_accumulator.hpp"
#include "../voice/note_event.hpp"
#include "../voice/ring_buffer.hpp"
#include "../voice/voice_manager.hpp"
#include <alsa/asoundlib.h>
#include <atomic>
#include <functional>
#include <random>
#include <string>
#include <thread>

class AudioEngine {
public:
	explicit AudioEngine(RingBuffer<NoteEvent, 64> &event_queue, SynthParams &params);
	~AudioEngine();

	bool open();
	void start();
	void stop();
	bool is_running() const { return running.load(); }

	std::function<void(float rms_l, float rms_r, float peak_l, float peak_r)> on_meter;
	std::function<void(WaveguideSnapshot)> on_waveguide;

	FftAccumulator<Config::FFT_ACC_SIZE> &get_fft_acc() { return fft_acc; }

private:
	std::string find_usb_device();
	void audio_loop();
	bool configure_device();

	snd_pcm_t *handle = nullptr;
	VoiceManager voice_manager;
	MasterBus master_bus;
	std::thread thread;
	std::atomic<bool> running {false};

	bool use_floats = true;

	RingBuffer<NoteEvent, 64> &event_queue;
	SynthParams &params;

	snd_pcm_uframes_t period_size = Config::PERIOD_SIZE;
	snd_pcm_uframes_t buffer_size = Config::BUFFER_SIZE;
	unsigned int sample_rate      = 48000;
	unsigned int channels         = Config::CHANNELS;

	std::vector<float> mix_l;
	std::vector<float> mix_r;
	std::vector<int8_t> buf;

	FftAccumulator<Config::FFT_ACC_SIZE> fft_acc;

	int meter_frame    = 0;
	int meter_interval = 1;
	float meter_rms_l  = 0;
	float meter_rms_r  = 0;
	float meter_peak_l = 0;
	float meter_peak_r = 0;

	std::default_random_engine generator;
	std::uniform_real_distribution<float> distribution {-1.0f, 1.0f};
};