#pragma once
#include <cstdint>
#include <initializer_list>

namespace Config {
// Audio
inline constexpr unsigned int SAMPLE_RATE = 48000;
inline constexpr unsigned int PERIOD_SIZE = 64;
inline constexpr unsigned int BUFFER_SIZE = PERIOD_SIZE * 4;
inline constexpr unsigned int CHANNELS    = 2;

// bit depth of the DAC
inline constexpr int BIT_DEPTH       = 16;
inline constexpr double SAMPLE_SCALE = 32767.0;
// static_cast<double>((1 << (BIT_DEPTH - 1)) - 1)
//     * static_cast<double>(1u << (32 - BIT_DEPTH));

// Devices
inline constexpr const char *AUDIO_DEVICE                         = "hw:A";
inline constexpr std::initializer_list<const char *> MIDI_DEVICES = {
    "KOMPLETE KONTROL",
    "Teensy MIDI",
};

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

inline constexpr float FILTER_KEYTRACK = 0.8f; // how much the filter cutoff tracks the note pitch
                                               // (0.0-2.0, where 1.0 means perfect tracking)

inline constexpr float KILL_MS = 1.5f; // ~64 samples at 44100Hz

inline constexpr float MIN_DECAY_MS = 10.0f;
inline constexpr float MAX_DECAY_MS = 15000.0f;

inline constexpr float SATURATION_DRIVE = 1.0f; // minimum 1.0f
} // namespace Config