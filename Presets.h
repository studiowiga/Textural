#pragma once
#include <JuceHeader.h>
#include "Parameters.h"

//==============================================================================
//  8 factory presets: 1-4 = green LEDs, 5-8 = red LEDs.
//  Values are in "real" units: 0..1 for knobs, 0/1 for switches, index for MODE.
//==============================================================================
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<juce::String, float>> values;
};

// Parameters that are global pedal settings (hold-footswitch layer) and are
// therefore NOT recalled by presets.
inline bool isGlobalSetting (const juce::String& id)
{
    return id == PID::outputMode || id == PID::trails || id == PID::fsHoldMode
        || id == PID::presetMute || id == PID::outputGain || id == PID::bypass || id == PID::freeze;
}

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> p {
        // ---------------------------------------------------------------- GREEN (1-4)
        { "Crystal Cave",   // regenerative octave loops cascading upward
          { { PID::mode, 0.0f }, { PID::time, 0.55f }, { PID::size, 0.45f }, { PID::overlap, 0.5f }, { PID::regen, 0.72f }, { PID::pitch, 0.75f }, { PID::mix, 0.5f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.12f }, { PID::sizeRnd, 0.2f }, { PID::overlapRnd, 0.25f }, { PID::regenMix, 0.85f }, { PID::pitchRnd, 0.1f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.4f }, { PID::reverbDecay, 0.72f }, { PID::cutoff, 0.55f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.05f } } },
        { "Melodic Kaleidoscope",   // frozen melodies replayed forward, reversed and at other octaves
          { { PID::mode, 1.0f }, { PID::time, 0.6f }, { PID::size, 0.4f }, { PID::overlap, 0.7f }, { PID::regen, 0.2f }, { PID::pitch, 0.5f }, { PID::mix, 0.6f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.45f }, { PID::sizeRnd, 0.3f }, { PID::overlapRnd, 0.35f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.55f }, { PID::reverseRnd, 0.4f }, { PID::reverbMix, 0.45f }, { PID::reverbDecay, 0.75f }, { PID::cutoff, 0.48f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.0f } } },
        { "Sea Synth",   // warbly polyphonic granular synth, 4 detuned voices
          { { PID::mode, 2.0f }, { PID::time, 0.3f }, { PID::size, 0.25f }, { PID::overlap, 0.9f }, { PID::regen, 0.8f }, { PID::pitch, 0.5f }, { PID::mix, 0.58f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.12f }, { PID::overlapRnd, 0.15f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.28f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.4f }, { PID::reverbDecay, 0.7f }, { PID::cutoff, 0.4f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.55f }, { PID::vibDepth, 0.55f } } },
        { "Obligatory Shimmer",   // granular shimmering reverb, octave-up cloud
          { { PID::mode, 0.0f }, { PID::time, 0.35f }, { PID::size, 0.7f }, { PID::overlap, 0.85f }, { PID::regen, 0.78f }, { PID::pitch, 0.75f }, { PID::mix, 0.45f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.35f }, { PID::sizeRnd, 0.35f }, { PID::overlapRnd, 0.4f }, { PID::regenMix, 1.0f }, { PID::pitchRnd, 0.12f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.55f }, { PID::reverbDecay, 0.88f }, { PID::cutoff, 0.56f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.25f }, { PID::vibDepth, 0.06f } } },
        // ------------------------------------------------------------------ RED (5-8)
        { "Reverse Reverb",   // reverb tails played backwards: swells that rise into each note
          { { PID::mode, 0.0f }, { PID::time, 0.55f }, { PID::size, 0.72f }, { PID::overlap, 0.42f }, { PID::regen, 0.3f }, { PID::pitch, 0.5f }, { PID::mix, 0.7f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.08f }, { PID::sizeRnd, 0.1f }, { PID::overlapRnd, 0.1f }, { PID::regenMix, 0.2f }, { PID::pitchRnd, 0.0f }, { PID::reverseRnd, 1.0f }, { PID::reverbMix, 0.7f }, { PID::reverbDecay, 0.8f }, { PID::cutoff, 0.5f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.0f } } },
        { "Stutter Pad",   // polyphonic pad with a bit of glitch (play staccato)
          { { PID::mode, 2.0f }, { PID::time, 0.15f }, { PID::size, 0.12f }, { PID::overlap, 0.45f }, { PID::regen, 0.75f }, { PID::pitch, 0.5f }, { PID::mix, 0.72f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.5f }, { PID::overlapRnd, 0.7f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.45f }, { PID::reverseRnd, 0.35f }, { PID::reverbMix, 0.35f }, { PID::reverbDecay, 0.6f }, { PID::cutoff, 0.58f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.0f } } },
        { "Sub Octave Ripple",   // quickly cascading sub-octave that adds depth and weight
          { { PID::mode, 0.0f }, { PID::time, 0.22f }, { PID::size, 0.2f }, { PID::overlap, 0.4f }, { PID::regen, 0.62f }, { PID::pitch, 0.25f }, { PID::mix, 0.5f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.08f }, { PID::sizeRnd, 0.12f }, { PID::overlapRnd, 0.15f }, { PID::regenMix, 0.9f }, { PID::pitchRnd, 0.0f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.2f }, { PID::reverbDecay, 0.45f }, { PID::cutoff, 0.4f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.0f } } },
        { "Mellow Cello",   // sub-octave pad with vibrato
          { { PID::mode, 1.0f }, { PID::time, 0.55f }, { PID::size, 0.55f }, { PID::overlap, 0.8f }, { PID::regen, 0.15f }, { PID::pitch, 0.25f }, { PID::mix, 0.6f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.25f }, { PID::sizeRnd, 0.2f }, { PID::overlapRnd, 0.2f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.12f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.35f }, { PID::reverbDecay, 0.65f }, { PID::cutoff, 0.32f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.8f }, { PID::vibDepth, 0.38f } } },
        // ------------------------------------------------- EXTRA (9) - not on the pedal slots
        { "Slow Attack Soundtrack",   // slowly evolving pad with random octave blips (DAW / menu only)
          { { PID::mode, 2.0f }, { PID::time, 0.82f }, { PID::size, 0.55f }, { PID::overlap, 0.85f }, { PID::regen, 0.9f }, { PID::pitch, 0.5f }, { PID::mix, 0.62f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.25f }, { PID::overlapRnd, 0.3f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.5f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.55f }, { PID::reverbDecay, 0.85f }, { PID::cutoff, 0.45f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.2f }, { PID::vibDepth, 0.08f } } },
    };
    return p;
}
