#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include <array>
#include <vector>
#include <atomic>

//==============================================================================
//  Textural granular engine (v3)
//
//  Modes (MODE button):
//   - Delay       : input written continuously; grains read TIME behind the head.
//   - Auto Freeze : each detected note/chord is recorded for TIME seconds, then the
//                   buffer freezes and grains wander through it endlessly.
//   - Layers      : up to 4 notes/chords in 4 buffers, each a "synth voice" with a
//                   fade-in (TIME) and fade-out (REGEN); 5th replaces the oldest.
//
//  Grain quality:
//   - adaptive raised-cosine window: Hann when grains overlap, a 20 % cross-fade
//     when OVERLAP is minimal, so consecutive grains always overlap -> no gaps;
//   - reversed grains use equal-power fades and only read audio that is already
//     recorded (never cross the write head) -> click-free, drop-out free reverse;
//   - 4-point Hermite interpolation; slight stereo decorrelation per grain;
//   - random "sowing" (position / size / spacing / pitch) to avoid comb ringing.
//
//  Feedback: delay feedback + granular feedback (REGEN, REGEN MIX), toned by the
//  filter knob, soft-limited, ping-pong in stereo, tape wow from vibrato depth.
//==============================================================================

struct EngineParams
{
    Mode    mode        = Mode::Delay;
    float   time        = 0.5f, size = 0.5f, overlap = 0.5f, regen = 0.3f, pitch = 0.5f;
    bool    reverse     = false;
    bool    freeze      = false;     // footswitch hold (Delay/Layers: freeze, AutoFreeze: un-freeze)
    float   timeRnd     = 0.15f, sizeRnd = 0.15f, overlapRnd = 0.15f;
    float   regenMix    = 0.5f, pitchRnd = 0.0f, reverseRnd = 0.5f;
    bool    quantize    = false;
    bool    stereo      = false;
    float   tapSpacing  = 0.0f;      // seconds, 0 = use OVERLAP
    bool    inputMuted  = false;     // trails while bypassed
    float   mix         = 0.5f;      // MIX above 60 % sprays particle grains
    float   cutoff      = 0.5f;      // filter knob, also colours the regenerated repeats
    float   vibDepth    = 0.0f;      // vibrato depth, also adds tape wow to the repeats
};

class GranularEngine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    // preL/preR (optional): signal written to the buffer instead of the input
    // (used for reverse reverb: reverb before the reversed grains)
    void process (const float* inL, const float* inR, const float* recL, const float* recR,
                  float* wetL, float* wetR, int numSamples, const EngineParams& p);

    std::atomic<bool> frozen { false };
    std::atomic<int>  layersActive { 0 };

private:
    static constexpr int kMaxGrains = 160;
    static constexpr int kLayers    = 4;

    struct Grain
    {
        bool   active = false;
        int    source = -1;           // -1 = main buffer, 0..3 = layer buffer
        double pos = 0, inc = 1;
        double stereoOffset = 0;      // right channel reads slightly apart (width)
        int    length = 1, age = 0;
        float  taper = 0.5f;          // fade fraction on each side
        bool   equalPower = false;    // reversed grains
        float  gL = 1, gR = 1;
        float  amp = 1;
    };

    struct Layer
    {
        enum Stage { Off, Attack, Sustain, Release };
        Stage  stage = Off;
        float  env = 0;
        bool   recording = false;
        int    recStart = 0, recCount = 0, recLen = 0, length = 0;
        double spawnCounter = 0;
        uint64_t order = 0;
        float  drift = 0;             // tiny per-voice detune (analog synth feel)
        std::vector<float> L, R;
    };

    inline float rnd()   { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (float) (seed & 0xFFFFFF) / 16777216.0f; }
    inline float brnd()  { return rnd() * 2.0f - 1.0f; }
    static inline float softClip (float x) { x = juce::jlimit (-3.0f, 3.0f, x); return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }

    static inline float window (float t, float taper, bool equalPower)
    {
        float w = 1.0f;
        if (t < taper)             w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t / taper);
        else if (t > 1.0f - taper) w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (1.0f - t) / taper);
        return equalPower ? std::sqrt (w) : w;
    }
    static inline float hermite (const float* b, int len, double pos)
    {
        const double fl = std::floor (pos);
        int i = (int) (fl - std::floor (fl / len) * len);
        const float f = (float) (pos - fl);
        const int im1 = i == 0 ? len - 1 : i - 1;
        const int i1  = i + 1 >= len ? i + 1 - len : i + 1;
        const int i2  = i + 2 >= len ? i + 2 - len : i + 2;
        const float y0 = b[im1], y1 = b[i], y2 = b[i1], y3 = b[i2];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    float  grainSemis (const EngineParams& p);
    float  effectiveDensity (const EngineParams& p) const;
    void   spawnGrain (const EngineParams& p, int source, double regionStart, double regionEnd, double anchor, float scatter);
    void   spawnParticle (const EngineParams& p);
    void   onsetDetected (const EngineParams& p);
    bool   detectOnset (float x);
    Grain* freeGrain();
    double alignToPrevious (const Grain& fresh, int fadeLen, double minShift, double maxShift, float& corrOut);

    double sr = 44100;
    std::vector<float> bufL, bufR;
    int    bufLen = 1, writePos = 0;
    std::array<Grain, kMaxGrains> grains;
    std::array<Layer, kLayers> layers;
    uint64_t layerCounter = 0;

    bool   afRecording = false, afHasSegment = false;
    int    afRecStart = 0, afRecCount = 0, afRecLen = 0;
    int    afSegStart = 0, afSegLen = 0;

    float  envFast = 0, envSlow = 0, noiseFloor = 0.001f, holdSeconds = 0.12f;
    float  fA = 0, fR = 0, sA = 0, sR = 0;
    int    holdOff = 0;

    double spawnCounter = 0, particleCounter = 0;
    float  timeSmooth = 0.3f;
    float  fbPrevL = 0, fbPrevR = 0, fbEnv = 0;
    float  fbLpL = 0, fbLpR = 0, fbHpL = 0, fbHpR = 0;
    float  wowPhase = 0;
    double lastStart = 0; int lastLen = 0; float lastRate = 1; bool haveLast = false;
    float  normGain = 1.0f, olaNorm = 1.0f;
    Mode   lastMode = Mode::Delay;
    uint32_t seed = 0x9E3779B9u;
};
