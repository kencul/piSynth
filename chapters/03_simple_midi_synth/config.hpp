#pragma once
#include <cstdint>

namespace Config {
// Audio
inline constexpr unsigned int SAMPLE_RATE = 44100;
inline constexpr unsigned int PERIOD_SIZE = 64;
inline constexpr unsigned int BUFFER_SIZE = PERIOD_SIZE * 4;
inline constexpr unsigned int CHANNELS    = 2;

// S32_LE: 24-bit precision packed into a 32-bit container
inline constexpr int32_t AMPLITUDE = 0x7FFFFF * 256;

// Devices
inline constexpr const char *AUDIO_DEVICE = "hw:UR22mkII";
inline constexpr const char *MIDI_DEVICE  = "KOMPLETE KONTROL";
} // namespace Config