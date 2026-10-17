#pragma once
#include <JuceHeader.h>

//==============================================================================
//  Textural (Studio Wiga) - parameter IDs, value mappings and MIDI CC map
//==============================================================================
namespace PID
{
    // Primary controls
    inline const juce::String time       { "time" };
    inline const juce::String size       { "size" };
    inline const juce::String overlap    { "overlap" };
    inline const juce::String regen      { "regen" };
    inline const juce::String pitch      { "pitch" };
    inline const juce::String mix        { "mix" };
    inline const juce::String reverse    { "reverse" };
    inline const juce::String mode       { "mode" };
    inline const juce::String bypass     { "bypass" };
    inline const juce::String freeze     { "freeze" };

    // Secondary - hold REVERSE
    inline const juce::String timeRnd    { "timeRandom" };
    inline const juce::String sizeRnd    { "sizeRandom" };
    inline const juce::String overlapRnd { "overlapRandom" };
    inline const juce::String regenMix   { "regenMix" };
    inline const juce::String pitchRnd   { "pitchRandom" };
    inline const juce::String reverseRnd { "reverseRandom" };

    // Secondary - hold MODE
    inline const juce::String reverbMix  { "reverbMix" };
    inline const juce::String reverbDecay{ "reverbDecay" };
    inline const juce::String cutoff     { "cutoff" };
    inline const juce::String quantize   { "quantize" };
    inline const juce::String vibRate    { "vibratoRate" };
    inline const juce::String vibDepth   { "vibratoDepth" };

    // Secondary - hold FOOTSWITCH
    inline const juce::String presetMute { "presetScrollMute" };
    inline const juce::String trails     { "trails" };
    inline const juce::String outputMode { "outputMode" };
    inline const juce::String fsHoldMode { "footswitchHoldMode" };
    inline const juce::String outputGain { "outputGain" };
}

enum class Mode { Delay = 0, AutoFreeze = 1, Layers = 2 };
enum class OutMode { Mono = 0, WetDry = 1, Stereo = 2 };

namespace Map
{
    // TIME : Delay/AutoFreeze -> delay time (and record length) 50 ms .. 2 s
    //        Layers           -> attack 5 ms .. 5 s
    inline float timeSeconds (float k, Mode m)
    {
        if (m == Mode::Layers) return 0.005f * std::pow (1000.0f, k);
        return 0.05f * std::pow (40.0f, k);
    }
    inline float timeToNorm (float seconds)       { return juce::jlimit (0.0f, 1.0f, std::log (seconds / 0.05f) / std::log (40.0f)); }

    // SIZE : Delay/AutoFreeze 60 ms .. 2 s, Layers 60 ms .. 500 ms
    inline float sizeSeconds (float k, Mode m)
    {
        if (m == Mode::Layers) return 0.06f * std::pow (0.5f / 0.06f, k);
        return 0.06f * std::pow (2.0f / 0.06f, k);
    }

    // OVERLAP : 1 .. 30 grains simultaneously
    inline float overlapCount (float k)          { return std::pow (30.0f, juce::jlimit (0.0f, 1.0f, k)); }

    // REGEN : feedback 0 .. ~100 % (Delay / AutoFreeze), decay 0.1 s .. 30 s / infinite (Layers)
    inline float feedback (float k)              { return juce::jlimit (0.0f, 0.985f, k * 0.985f); }
    inline float layerDecaySeconds (float k)     { return k >= 0.985f ? 1.0e9f : 0.1f * std::pow (300.0f, k); }

    // PITCH : -24 .. +24 semitones with a small centre detent
    inline float pitchSemis (float k)
    {
        const float c = k - 0.5f;
        if (std::abs (c) < 0.012f) return 0.0f;
        return juce::jlimit (-24.0f, 24.0f, (c - (c > 0 ? 0.012f : -0.012f)) / (0.5f - 0.012f) * 24.0f);
    }

    inline float quantiseSemis (float s)
    {
        static const float steps[] = { -24, -19, -17, -12, -7, -5, 0, 5, 7, 12, 17, 19, 24 };
        float best = 0, bestD = 1000;
        for (auto v : steps) { auto d = std::abs (v - s); if (d < bestD) { bestD = d; best = v; } }
        return best;
    }

    // MIX : full dry .. full wet, both at unity at noon
    inline float dryGain (float k)               { return juce::jmin (1.0f, 2.0f * (1.0f - k)); }
    inline float wetGain (float k)               { return juce::jmin (1.0f, 2.0f * k); }

    inline float reverseProbability (float k)    { return 0.1f + 0.9f * k; }
    inline float reverbRT60 (float k)            { return k >= 0.97f ? 1.0e6f : 0.4f * std::pow (150.0f, k / 0.97f); }   // 0.4 s .. 60 s, then infinite
    inline float vibratoHz (float k)             { return 0.1f * std::pow (100.0f, k); }
    inline float vibratoMs (float k)             { return 6.0f * k * k; }
    inline float outputDb (float k)              { return (k - 0.5f) * 24.0f; }
}

//==============================================================================
// Fixed MIDI CC map (also shown in the status line and README)
//==============================================================================
struct CCEntry { int cc; const juce::String* id; };

inline const std::vector<CCEntry>& ccMap()
{
    static const std::vector<CCEntry> m {
        { 14, &PID::time },      { 15, &PID::size },       { 16, &PID::overlap },
        { 17, &PID::regen },     { 18, &PID::pitch },      { 19, &PID::mix },
        { 20, &PID::timeRnd },   { 21, &PID::sizeRnd },    { 22, &PID::overlapRnd },
        { 23, &PID::regenMix },  { 24, &PID::pitchRnd },   { 25, &PID::reverseRnd },
        { 26, &PID::reverbMix }, { 27, &PID::reverbDecay },{ 28, &PID::cutoff },
        { 29, &PID::quantize },  { 30, &PID::vibRate },    { 31, &PID::vibDepth },
        { 80, &PID::reverse },   { 81, &PID::mode },       { 82, &PID::bypass },
        { 64, &PID::freeze },    { 85, &PID::outputGain }, { 86, &PID::outputMode },
        { 87, &PID::trails },    { 88, &PID::fsHoldMode }, { 89, &PID::presetMute }
    };
    return m;
}
inline constexpr int kCCTap        = 83;   // tap tempo (value >= 64)
inline constexpr int kCCPresetNext = 84;   // next preset (value >= 64)

inline int ccForParam (const juce::String& id)
{
    for (auto& e : ccMap()) if (*e.id == id) return e.cc;
    return -1;
}
