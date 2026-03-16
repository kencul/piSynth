#pragma once
#include <cstdint>

namespace Config {
// Audio
inline constexpr unsigned int SAMPLE_RATE = 44100;
inline constexpr unsigned int PERIOD_SIZE = 64;
inline constexpr unsigned int BUFFER_SIZE = PERIOD_SIZE * 4;
inline constexpr unsigned int CHANNELS    = 2;

// bit depth of the DAC
inline constexpr int BIT_DEPTH = 24;
inline constexpr double SAMPLE_SCALE =
    static_cast<double>((1 << (BIT_DEPTH - 1)) - 1) * static_cast<double>(1u << (32 - BIT_DEPTH));

// Devices
inline constexpr const char *AUDIO_DEVICE = "hw:UR22mkII";
inline constexpr const char *MIDI_DEVICE  = "KOMPLETE KONTROL";

inline constexpr int MAX_VOICES = 8;

// ADSR defaults
inline constexpr float DEFAULT_RELEASE = 100.0f; // ms

inline constexpr float KILL_MS = 1.5f; // ~64 samples at 44100Hz

inline constexpr float DEFAULT_DECAY_MS = 30000.0f; // time to reach -60dB

inline constexpr float GAIN_SMOOTH_MS   = 3000.0f; // gain rise time
inline constexpr float GAIN_ATTACK_MS   = 10.0f;   // gain drop time
inline constexpr float SATURATION_DRIVE = 1.0f;    // minimum 1.0f
} // namespace Config