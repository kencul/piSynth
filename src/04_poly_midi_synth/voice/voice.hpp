#pragma once
#include "../osc/osc.hpp"
#include "../adsr/adsr.hpp"
#include "../config.hpp"

struct Voice {
    Oscillator osc          { Config::SAMPLE_RATE };
    ADSR       envelope     { Config::SAMPLE_RATE };
    int        note         = -1;
    bool       active       = false;
    float      velocity_gain = 1.0f;

    void trigger(int midi_note, double hz, int velocity)
    {
        note          = midi_note;
        active        = true;
        velocity_gain = velocity / 127.0f;
        osc.set_frequency(hz);
        envelope.trigger();
    }

    void release()
    {
        envelope.release();
    }
};