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
        { "Crystal Cascade",   // octave-up granular regen falling into a lush shimmer
          { { PID::mode, 0.0f }, { PID::time, 0.56f }, { PID::size, 0.46f }, { PID::overlap, 0.62f }, { PID::regen, 0.8f }, { PID::pitch, 0.75f }, { PID::mix, 0.55f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.12f }, { PID::sizeRnd, 0.2f }, { PID::overlapRnd, 0.2f }, { PID::regenMix, 0.92f }, { PID::pitchRnd, 0.12f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.55f }, { PID::reverbDecay, 0.8f }, { PID::cutoff, 0.55f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.06f } } },
        { "Particle Rain",   // sparkling particle cloud, reversed dust, octaves
          { { PID::mode, 0.0f }, { PID::time, 0.5f }, { PID::size, 0.12f }, { PID::overlap, 0.55f }, { PID::regen, 0.55f }, { PID::pitch, 0.5f }, { PID::mix, 0.88f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.42f }, { PID::sizeRnd, 0.6f }, { PID::overlapRnd, 0.6f }, { PID::regenMix, 0.6f }, { PID::pitchRnd, 0.62f }, { PID::reverseRnd, 0.45f }, { PID::reverbMix, 0.6f }, { PID::reverbDecay, 0.84f }, { PID::cutoff, 0.6f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.35f }, { PID::vibDepth, 0.0f } } },
        { "Nebula Pad",   // detuned polyphonic synth pad, slow swell, endless hall
          { { PID::mode, 2.0f }, { PID::time, 0.72f }, { PID::size, 0.45f }, { PID::overlap, 0.9f }, { PID::regen, 0.93f }, { PID::pitch, 0.5f }, { PID::mix, 0.7f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.1f }, { PID::overlapRnd, 0.12f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.3f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.62f }, { PID::reverbDecay, 0.9f }, { PID::cutoff, 0.38f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.22f }, { PID::vibDepth, 0.12f } } },
        { "Tenor Sax",   // breathy swell, nasal resonant tone and 5 Hz vibrato
          { { PID::mode, 2.0f }, { PID::time, 0.38f }, { PID::size, 0.22f }, { PID::overlap, 0.86f }, { PID::regen, 0.78f }, { PID::pitch, 0.5f }, { PID::mix, 0.6f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.05f }, { PID::overlapRnd, 0.08f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.07f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.24f }, { PID::reverbDecay, 0.45f }, { PID::cutoff, 0.3f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.85f }, { PID::vibDepth, 0.45f } } },
        // ------------------------------------------------------------------ RED (5-8)
        { "Pure Reverse",   // 100 % backwards: every grain reversed, wet only
          { { PID::mode, 0.0f }, { PID::time, 0.65f }, { PID::size, 0.76f }, { PID::overlap, 0.36f }, { PID::regen, 0.38f }, { PID::pitch, 0.5f }, { PID::mix, 1.0f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.0f }, { PID::overlapRnd, 0.05f }, { PID::regenMix, 0.0f }, { PID::pitchRnd, 0.0f }, { PID::reverseRnd, 1.0f }, { PID::reverbMix, 0.22f }, { PID::reverbDecay, 0.55f }, { PID::cutoff, 0.5f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.35f }, { PID::vibDepth, 0.0f } } },
        { "Glitch Engine",   // stuttering micro-slices, random repeats and pitch jumps
          { { PID::mode, 0.0f }, { PID::time, 0.46f }, { PID::size, 0.06f }, { PID::overlap, 0.3f }, { PID::regen, 0.55f }, { PID::pitch, 0.5f }, { PID::mix, 0.8f }, { PID::reverse, 1.0f }, { PID::timeRnd, 0.85f }, { PID::sizeRnd, 0.72f }, { PID::overlapRnd, 0.75f }, { PID::regenMix, 0.4f }, { PID::pitchRnd, 0.55f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.15f }, { PID::reverbDecay, 0.4f }, { PID::cutoff, 0.6f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.35f }, { PID::vibDepth, 0.0f } } },
        { "Tape Echoes",   // warm ping-pong tape delay with wow and dark repeats
          { { PID::mode, 0.0f }, { PID::time, 0.6f }, { PID::size, 0.9f }, { PID::overlap, 0.9f }, { PID::regen, 0.72f }, { PID::pitch, 0.5f }, { PID::mix, 0.5f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.0f }, { PID::sizeRnd, 0.04f }, { PID::overlapRnd, 0.05f }, { PID::regenMix, 0.0f }, { PID::pitchRnd, 0.04f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.3f }, { PID::reverbDecay, 0.6f }, { PID::cutoff, 0.36f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.3f }, { PID::vibDepth, 0.32f } } },
        { "Frozen Cathedral",   // auto-frozen chords with octaves in an infinite reverb
          { { PID::mode, 1.0f }, { PID::time, 0.62f }, { PID::size, 0.62f }, { PID::overlap, 0.85f }, { PID::regen, 0.3f }, { PID::pitch, 0.5f }, { PID::mix, 0.68f }, { PID::reverse, 0.0f }, { PID::timeRnd, 0.3f }, { PID::sizeRnd, 0.3f }, { PID::overlapRnd, 0.3f }, { PID::regenMix, 0.5f }, { PID::pitchRnd, 0.42f }, { PID::reverseRnd, 0.5f }, { PID::reverbMix, 0.66f }, { PID::reverbDecay, 1.0f }, { PID::cutoff, 0.44f }, { PID::quantize, 1.0f }, { PID::vibRate, 0.2f }, { PID::vibDepth, 0.08f } } },
    };
    return p;
}
