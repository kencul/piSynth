#pragma once
#include <cstdint>
#include <initializer_list>

namespace Config {
// Audio
inline unsigned int SAMPLE_RATE =
    44100; // not constecpr because ALSA sample rate is determined at runtime
inline constexpr unsigned int PERIOD_SIZE = 64;
inline constexpr unsigned int BUFFER_SIZE = PERIOD_SIZE * 4;
inline constexpr unsigned int CHANNELS    = 2;

// bit depth of the DAC
inline constexpr double SAMPLE_SCALE = 32767.0;
// static_cast<double>((1 << (BIT_DEPTH - 1)) - 1)
//     * static_cast<double>(1u << (32 - BIT_DEPTH));

// Devices
inline constexpr const char *AUDIO_DEVICE                         = "hw:A";
inline constexpr std::initializer_list<const char *> MIDI_DEVICES = {
    "KOMPLETE KONTROL",
    "Teensy MIDI",
};

// ------- Synth Parameters -------

inline constexpr int MAX_VOICES = 8;

// ADSR Envelope
inline constexpr float MAX_ATTACK_TIME  = 50.0f;   // ms
inline constexpr float MAX_RELEASE_TIME = 1000.0f; // ms

// String Valules
inline constexpr float PLUCK_POS      = 0.2f; // 0-1, relative position of the pluck on the string
inline constexpr float PICKUP_POS     = 0.1f; // 0-1
inline constexpr float MIN_PICKUP_POS = 0.05f;

// Voice Panning
inline constexpr float PAN_SPREAD =
    0.2f; // how far left/right the highest/lowest notes pan based on pitch
inline constexpr float PAN_SEMITONES =
    48.0f; // semitones from center note (E4, MIDI note 64) to reach max pan

inline constexpr float KILL_MS = 1.5f; // ~64 samples at 44100Hz

inline constexpr float MIN_DECAY_MS = 10.0f;
inline constexpr float MAX_DECAY_MS = 15000.0f;

// ----- FFT -------
inline constexpr int FFT_SIZE     = 4096;
inline constexpr int FFT_ACC_SIZE = 8192;
inline constexpr int FFT_OUT_BINS = 512; // downsampled display bins sent to the UI

// ----- Effects -------

// Saturation
inline constexpr float SATURATION_DRIVE = 1.0f; // minimum 1.0f

// Filter
inline constexpr float FILTER_KEYTRACK = 0.8f; // how much the filter cutoff tracks the note pitch
                                               // (0.0-2.0, where 1.0 means perfect tracking)
// Chorus
inline constexpr float CHORUS_LEFT_BASE_MS   = 13.0f;
inline constexpr float CHORUS_RIGHT_BASE_MS  = 9.0f;
inline constexpr float CHORUS_DEPTH_COUPLING = 2.0f; // depth_ms = coupling / rate_hz
inline constexpr float CHORUS_MIN_RATE_HZ    = 1.0f;
inline constexpr float CHORUS_MAX_RATE_HZ    = 5.0f;
inline constexpr float CHORUS_MAX_DEPTH_MULT = 2.0f;
// derived: largest possible read position across all param combinations
inline constexpr float CHORUS_MAX_DELAY_MS = CHORUS_LEFT_BASE_MS
    + (CHORUS_DEPTH_COUPLING / CHORUS_MIN_RATE_HZ) * CHORUS_MAX_DEPTH_MULT + 1.0f;
inline constexpr float CHORUS_LFO_RATE_MULT_OFFSET =
    0.2f; // make left and right LFO rates different to create a wider stereo effect

// Ping Pong Delay
inline constexpr float PING_PONG_MAX_DELAY_MS       = 1000.0f;
inline constexpr float PING_PONG_FEEDBACK_CUTOFF_HZ = 6000.0f;

// Reverb
inline constexpr float REVERB_MIN_CUTOFF_HZ = 1000.0f;
inline constexpr float REVERB_MAX_CUTOFF_HZ = 10000.0f;

// ------- Web UI -------

inline constexpr int UI_UPDATES_PER_SECOND = 60;
inline constexpr int UI_PORT               = 9002;
} // namespace Config