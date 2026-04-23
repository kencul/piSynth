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
#include <thread>

class AudioEngine {
public:
	explicit AudioEngine(RingBuffer<NoteEvent, 64> &event_queue, SynthParams &params);
	~AudioEngine();

	bool open(const char *device);
	void start();
	void stop();

	std::function<void(float rms_l, float rms_r, float peak_l, float peak_r)> on_meter;
	std::function<void(WaveguideSnapshot)> on_waveguide;

	FftAccumulator<8192> &get_fft_acc() { return fft_acc; }

private:
	void audio_loop();
	bool configure_device();

	snd_pcm_t *handle = nullptr;
	VoiceManager voice_manager;
	MasterBus master_bus;
	std::thread thread;
	std::atomic<bool> running {false};

	RingBuffer<NoteEvent, 64> &event_queue;
	SynthParams &params;

	snd_pcm_uframes_t period_size = Config::PERIOD_SIZE;
	snd_pcm_uframes_t buffer_size = Config::BUFFER_SIZE;
	unsigned int sample_rate      = Config::SAMPLE_RATE;
	unsigned int channels         = Config::CHANNELS;

	std::vector<float> mix_l;
	std::vector<float> mix_r;
	std::vector<int16_t> buf;

	FftAccumulator<8192> fft_acc;

	int meter_frame    = 0;
	int meter_interval = 25;
	float meter_rms_l  = 0;
	float meter_rms_r  = 0;
	float meter_peak_l = 0;
	float meter_peak_r = 0;
};