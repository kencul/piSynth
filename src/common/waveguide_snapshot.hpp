#pragma once
#include <array>

struct WaveguideSnapshot {
    static constexpr int POINTS = 128;

    // Physical string displacement (forward + backward wave superposition) resampled to POINTS.
    std::array<float, POINTS> displacement {};

    // Normalized length of the vibrating string: half_delay_len / (MAX_DELAY/2).
    // Maps to fret position — higher notes produce a smaller value.
    float fret_pos   = 0.0f;

    // Normalized pickup position within the vibrating length (0=nut, 1=bridge).
    float pickup_pos = 0.0f;

    bool active = false;
};
