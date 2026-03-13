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

inline constexpr int MAX_VOICES = 4;

// ADSR defaults
inline constexpr float DEFAULT_ATTACK  = 10.0f;  // ms
inline constexpr float DEFAULT_DECAY   = 100.0f; // ms
inline constexpr float DEFAULT_SUSTAIN = 0.7f;   // level 0-1
inline constexpr float DEFAULT_RELEASE = 300.0f; // ms
} // namespace Config