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
        { "Sea Synth",   // warbly polyphonic granular synth, 4 detuned voices
          { { PID::mode, 2.0f }, { PID::time, 0.3f }, { PID::size, 0.25f }, { PID::overlap, 0.9f }, { PID::regen, 0.8f }, { PID::pitch, 0.5f }, { PID::mix, 0.58f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.12f }, { PID::overlapRnd, 0.15f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.28f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.4f }, { PID::reverbDecay, 0.7f }, { PID::cutoff, 0.4f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.55f }, { PID::vibDepth, 0.55f } } },
        { "Live",   // position 2 = Live mode (current knob settings), never recalled from here
          { } },
        { "Crystal Cave",   // regenerative octave loops cascading upward
          { { PID::mode, 0.0f }, { PID::time, 0.55f }, { PID::size, 0.45f }, { PID::overlap, 0.5f }, { PID::regen, 0.72f }, { PID::pitch, 0.75f }, { PID::mix, 0.5f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.12f }, { PID::sizeRnd, 0.2f }, { PID::overlapRnd, 0.25f }, { PID::regenMix, 0.85f }, { PID::pitchRnd, 0.1f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.4f }, { PID::reverbDecay, 0.72f }, { PID::cutoff, 0.55f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.05f } } },
        { "Melodic Kaleidoscope",   // frozen melodies replayed forward, reversed and at other octaves
          { { PID::mode, 1.0f }, { PID::time, 0.6f }, { PID::size, 0.4f }, { PID::overlap, 0.7f }, { PID::regen, 0.2f }, { PID::pitch, 0.5f }, { PID::mix, 0.5f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.45f }, { PID::sizeRnd, 0.3f }, { PID::overlapRnd, 0.35f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.55f }, { PID::reverseRnd, 0.4f }, { PID::reverbMix, 0.38f }, { PID::reverbDecay, 0.75f }, { PID::cutoff, 0.48f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.0f } } },
        // ------------------------------------------------------------------ RED (5-8)
        { "Obligatory Shimmer",   // granular shimmering reverb, octave-up cloud
          { { PID::mode, 0.0f }, { PID::time, 0.35f }, { PID::size, 0.7f }, { PID::overlap, 0.85f }, { PID::regen, 0.78f }, { PID::pitch, 0.75f }, { PID::mix, 0.45f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.35f }, { PID::sizeRnd, 0.35f }, { PID::overlapRnd, 0.4f }, { PID::regenMix, 0.17f }, { PID::pitchRnd, 0.12f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.55f }, { PID::reverbDecay, 0.88f }, { PID::cutoff, 0.56f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.25f }, { PID::vibDepth, 0.06f } } },
        { "Reverse Reverb",   // reverse echo: each ~0.9 s of guitar replayed backwards, then into the reverb
          { { PID::mode, 0.0f }, { PID::time, 0.0f }, { PID::size, 0.78f }, { PID::overlap, 0.0f }, { PID::regen, 0.2f }, { PID::pitch, 0.5f }, { PID::mix, 0.72f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.0f }, { PID::overlapRnd, 0.0f }, { PID::regenMix, 0.0f }, { PID::pitchRnd, 0.0f }, { PID::reverseRnd, 1.0f }, { PID::reverbMix, 0.4f }, { PID::reverbDecay, 0.7f }, { PID::cutoff, 0.5f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.0f } } },
        { "Mellow Cello",   // sub-octave pad with vibrato, swelling up loud after each note
          { { PID::mode, 1.0f }, { PID::time, 0.5f }, { PID::size, 0.55f }, { PID::overlap, 0.8f }, { PID::regen, 0.15f }, { PID::pitch, 0.25f }, { PID::mix, 0.74f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.25f }, { PID::sizeRnd, 0.2f }, { PID::overlapRnd, 0.2f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.1f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.28f }, { PID::reverbDecay, 0.62f }, { PID::cutoff, 0.38f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.8f }, { PID::vibDepth, 0.38f } } },
        { "Stardust Cloud",   // granular synth: detuned Layers voices showered with sparkling particles
          { { PID::mode, 2.0f }, { PID::time, 0.45f }, { PID::size, 0.3f }, { PID::overlap, 0.9f }, { PID::regen, 0.86f }, { PID::pitch, 0.5f }, { PID::mix, 0.86f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.4f }, { PID::overlapRnd, 0.4f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.45f }, { PID::reverseRnd, 0.3f }, { PID::reverbMix, 0.5f }, { PID::reverbDecay, 0.82f }, { PID::cutoff, 0.5f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.4f }, { PID::vibDepth, 0.2f } } },
        // ------------------------------------------------- EXTRA (9) - not on the pedal slots
        { "Slow Attack Soundtrack",   // slowly evolving pad with random octave blips (DAW / menu only)
          { { PID::mode, 2.0f }, { PID::time, 0.68989f }, { PID::size, 0.57486f }, { PID::overlap, 0.84981f }, { PID::regen, 0.90809f }, { PID::pitch, 0.34567f }, { PID::mix, 0.38f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.25f }, { PID::overlapRnd, 0.3f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.5f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.55f }, { PID::reverbDecay, 0.85f }, { PID::cutoff, 0.45f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.2f }, { PID::vibDepth, 0.08f } } },
    };
    return p;
}
