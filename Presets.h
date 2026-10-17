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
        // ---------------------------------------------------------------- GREEN
        { "Glass Grotto",      // regenerative octave-up loops
          { { PID::mode, 0 }, { PID::time, 0.58f }, { PID::size, 0.42f }, { PID::overlap, 0.42f },
            { PID::regen, 0.72f }, { PID::pitch, 0.75f }, { PID::mix, 0.5f }, { PID::reverse, 0 },
            { PID::timeRnd, 0.1f }, { PID::sizeRnd, 0.12f }, { PID::overlapRnd, 0.15f },
            { PID::regenMix, 0.88f }, { PID::pitchRnd, 0.0f }, { PID::reverseRnd, 0.5f },
            { PID::reverbMix, 0.3f }, { PID::reverbDecay, 0.55f }, { PID::cutoff, 0.5f },
            { PID::quantize, 0 }, { PID::vibRate, 0.35f }, { PID::vibDepth, 0.0f } } },

        { "Kaleidoscope",      // frozen melodies replayed forward, reversed and at octaves
          { { PID::mode, 1 }, { PID::time, 0.6f }, { PID::size, 0.38f }, { PID::overlap, 0.6f },
            { PID::regen, 0.25f }, { PID::pitch, 0.5f }, { PID::mix, 0.55f }, { PID::reverse, 1 },
            { PID::timeRnd, 0.45f }, { PID::sizeRnd, 0.3f }, { PID::overlapRnd, 0.3f },
            { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.55f }, { PID::reverseRnd, 0.45f },
            { PID::reverbMix, 0.4f }, { PID::reverbDecay, 0.6f }, { PID::cutoff, 0.5f },
            { PID::quantize, 1 }, { PID::vibRate, 0.35f }, { PID::vibDepth, 0.0f } } },

        { "Tidal Synth",       // warbly polyphonic synth
          { { PID::mode, 2 }, { PID::time, 0.25f }, { PID::size, 0.22f }, { PID::overlap, 0.85f },
            { PID::regen, 0.72f }, { PID::pitch, 0.5f }, { PID::mix, 0.68f }, { PID::reverse, 0 },
            { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.08f }, { PID::overlapRnd, 0.1f },
            { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.0f }, { PID::reverseRnd, 0.5f },
            { PID::reverbMix, 0.3f }, { PID::reverbDecay, 0.45f }, { PID::cutoff, 0.4f },
            { PID::quantize, 0 }, { PID::vibRate, 0.5f }, { PID::vibDepth, 0.5f } } },

        { "Halo Shimmer",      // granular shimmering reverb
          { { PID::mode, 0 }, { PID::time, 0.35f }, { PID::size, 0.68f }, { PID::overlap, 0.82f },
            { PID::regen, 0.78f }, { PID::pitch, 0.75f }, { PID::mix, 0.45f }, { PID::reverse, 0 },
            { PID::timeRnd, 0.35f }, { PID::sizeRnd, 0.35f }, { PID::overlapRnd, 0.35f },
            { PID::regenMix, 1.0f }, { PID::pitchRnd, 0.1f }, { PID::reverseRnd, 0.5f },
            { PID::reverbMix, 0.62f }, { PID::reverbDecay, 0.82f }, { PID::cutoff, 0.56f },
            { PID::quantize, 1 }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.12f } } },

        // ------------------------------------------------------------------ RED
        { "Glitch Stutter",    // short reversed blips, rhythmic chaos
          { { PID::mode, 0 }, { PID::time, 0.42f }, { PID::size, 0.08f }, { PID::overlap, 0.18f },
            { PID::regen, 0.45f }, { PID::pitch, 0.5f }, { PID::mix, 0.6f }, { PID::reverse, 1 },
            { PID::timeRnd, 0.65f }, { PID::sizeRnd, 0.55f }, { PID::overlapRnd, 0.6f },
            { PID::regenMix, 0.3f }, { PID::pitchRnd, 0.35f }, { PID::reverseRnd, 0.35f },
            { PID::reverbMix, 0.18f }, { PID::reverbDecay, 0.3f }, { PID::cutoff, 0.62f },
            { PID::quantize, 1 }, { PID::vibRate, 0.35f }, { PID::vibDepth, 0.0f } } },

        { "Slow Horizon",      // slowly evolving pad with random octave blips
          { { PID::mode, 2 }, { PID::time, 0.78f }, { PID::size, 0.65f }, { PID::overlap, 0.78f },
            { PID::regen, 0.86f }, { PID::pitch, 0.5f }, { PID::mix, 0.6f }, { PID::reverse, 0 },
            { PID::timeRnd, 0.2f }, { PID::sizeRnd, 0.3f }, { PID::overlapRnd, 0.3f },
            { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.3f }, { PID::reverseRnd, 0.5f },
            { PID::reverbMix, 0.55f }, { PID::reverbDecay, 0.78f }, { PID::cutoff, 0.42f },
            { PID::quantize, 1 }, { PID::vibRate, 0.25f }, { PID::vibDepth, 0.1f } } },

        { "Sub Ripple",        // quickly cascading sub-octave
          { { PID::mode, 0 }, { PID::time, 0.22f }, { PID::size, 0.18f }, { PID::overlap, 0.38f },
            { PID::regen, 0.62f }, { PID::pitch, 0.25f }, { PID::mix, 0.5f }, { PID::reverse, 0 },
            { PID::timeRnd, 0.1f }, { PID::sizeRnd, 0.1f }, { PID::overlapRnd, 0.12f },
            { PID::regenMix, 0.92f }, { PID::pitchRnd, 0.0f }, { PID::reverseRnd, 0.5f },
            { PID::reverbMix, 0.15f }, { PID::reverbDecay, 0.4f }, { PID::cutoff, 0.38f },
            { PID::quantize, 1 }, { PID::vibRate, 0.35f }, { PID::vibDepth, 0.0f } } },

        { "Velvet Cello",      // sub-octave pad with vibrato
          { { PID::mode, 1 }, { PID::time, 0.55f }, { PID::size, 0.55f }, { PID::overlap, 0.72f },
            { PID::regen, 0.2f }, { PID::pitch, 0.25f }, { PID::mix, 0.6f }, { PID::reverse, 0 },
            { PID::timeRnd, 0.25f }, { PID::sizeRnd, 0.2f }, { PID::overlapRnd, 0.2f },
            { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.0f }, { PID::reverseRnd, 0.5f },
            { PID::reverbMix, 0.32f }, { PID::reverbDecay, 0.6f }, { PID::cutoff, 0.33f },
            { PID::quantize, 1 }, { PID::vibRate, 0.42f }, { PID::vibDepth, 0.38f } } },
    };
    return p;
}
