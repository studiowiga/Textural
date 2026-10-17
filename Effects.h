#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

//==============================================================================
//  Small DSP building blocks: TPT state-variable filter, 4-pole dual LP/HP
//  filter, vibrato (modulated delay) and an 8-line FDN stereo reverb.
//==============================================================================

struct SVF
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1.4142f, a1 = 0, a2 = 0, a3 = 0;

    void set (float fc, float sr, float q)
    {
        fc = juce::jlimit (10.0f, 0.45f * sr, fc);
        g  = std::tan (juce::MathConstants<float>::pi * fc / sr);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }

    inline void tick (float x, float& lp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        hp = x - k * v1 - v2;
    }
};

// 4-pole (24 dB/oct) dual filter: knob < noon = low-pass, > noon = high-pass, noon = flat
class DualFilter
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; reset(); smoothed = 0.5f; }
    void reset() { for (auto& c : st) for (auto& f : c) f.reset(); }

    void setKnob (float k) { target = k; }

    void process (float& l, float& r)
    {
        smoothed += 0.0015f * (target - smoothed);
        if (++counter >= 16) { counter = 0; update(); }
        if (flat) return;
        l = run (0u, l);
        r = run (1u, r);
    }

private:
    void update()
    {
        const float k = smoothed;
        if (std::abs (k - 0.5f) < 0.015f) { flat = true; return; }
        flat = false;
        isHP = k > 0.5f;
        float fc;
        if (! isHP) fc = 180.0f * std::pow (20000.0f / 180.0f, k / 0.5f);           // 180 Hz .. 20 kHz
        else        fc = 20.0f  * std::pow (4000.0f / 20.0f, (k - 0.5f) / 0.5f);    // 20 Hz .. 4 kHz
        // resonance grows as the knob leaves noon: synth-like sweeps
        const float res = 1.3066f + 3.2f * std::pow (std::abs (k - 0.5f) * 2.0f, 1.5f);
        for (auto& c : st) { c[0].set (fc, sr, 0.5412f); c[1].set (fc, sr, res); }
    }
    float run (size_t ch, float x)
    {
        float lp, hp;
        st[ch][0].tick (x, lp, hp);   x = isHP ? hp : lp;
        st[ch][1].tick (x, lp, hp);   return isHP ? hp : lp;
    }

    std::array<std::array<SVF, 2>, 2> st;
    float sr = 44100, target = 0.5f, smoothed = 0.5f;
    bool flat = true, isHP = false;
    int counter = 0;
};

//==============================================================================
class Vibrato
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        size = (int) (sr * 0.03f) + 4;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        w = 0; phase = 0;
    }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }

    void set (float rateHz, float depthMs) { rate = rateHz; depthT = depthMs; }

    void process (float& l, float& r)
    {
        depth += 0.0008f * (depthT - depth);
        buf[0][(size_t) w] = l;
        buf[1][(size_t) w] = r;
        if (depth > 0.01f)
        {
            phase += rate / sr;
            if (phase >= 1.0f) phase -= 1.0f;
            const float d = (depth * (1.0f + std::sin (juce::MathConstants<float>::twoPi * phase)) * 0.5f + 0.25f) * sr * 0.001f;
            l = read (0, d);
            r = read (1, d);
        }
        if (++w >= size) w = 0;
    }

private:
    float read (int ch, float d) const
    {
        float p = (float) w - d;
        while (p < 0) p += (float) size;
        int i0 = (int) p;
        if (i0 >= size) { i0 -= size; p -= (float) size; }
        const int i1 = (i0 + 1) % size;
        const float f = p - (float) i0;
        return buf[ch][(size_t) i0] + f * (buf[ch][(size_t) i1] - buf[ch][(size_t) i0]);
    }

    std::vector<float> buf[2];
    float sr = 44100, rate = 1, depth = 0, depthT = 0, phase = 0;
    int size = 1, w = 0;
};

//==============================================================================
// Lush "ambient" FDN reverb: 8 modulated delay lines (chorused tail), 6-stage
// input diffusion (bloom), damping linked to the dual filter, infinite hold at
// the top of the Reverb Decay range.
class FDNReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        static const float ms[8] = { 43.1f, 53.7f, 61.9f, 73.3f, 83.9f, 97.1f, 109.7f, 127.3f };
        maxMod = (int) (0.0035f * sr) + 4;
        for (int i = 0; i < 8; ++i)
        {
            len[i] = (int) (ms[i] * 0.001f * sr);
            size[i] = len[i] + maxMod + 4;
            line[i].assign ((size_t) size[i], 0.0f);
            pos[i] = 0;
            lp[i] = 0;
            lfo[i] = (float) i * 0.37f;
            lfoInc[i] = (0.11f + 0.047f * (float) i) / sr;
        }
        static const float apMs[6] = { 4.77f, 3.59f, 12.73f, 9.31f, 23.1f, 17.9f };
        for (int i = 0; i < 6; ++i)
        {
            apLen[i] = (int) (apMs[i] * 0.001f * sr);
            ap[i].assign ((size_t) apLen[i] + 1, 0.0f);
            apPos[i] = 0;
        }
        currentRT = -1;
        setDecay (3.0f);
    }

    void reset()
    {
        for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& a : ap)   std::fill (a.begin(), a.end(), 0.0f);
        for (auto& v : lp) v = 0;
    }

    // tone: 0 = dark, 0.5 = neutral, 1 = bright
    void setTone (float t) { tone = juce::jlimit (0.0f, 1.0f, t); currentRT = -1; }

    void setDecay (float rt60)
    {
        if (std::abs (rt60 - currentRT) < 0.01f) return;
        currentRT = rt60;
        infinite = rt60 > 100.0f;
        for (int i = 0; i < 8; ++i)
            gain[i] = infinite ? 0.99985f : std::pow (10.0f, -3.0f * (float) len[i] / (rt60 * sr));
        const float baseDamp = infinite ? 0.55f : juce::jlimit (0.12f, 0.75f, 0.6f - 0.012f * rt60);
        damp = juce::jlimit (0.06f, 0.95f, baseDamp * (0.45f + 1.1f * tone));
        modDepth = juce::jlimit (0.0f, (float) maxMod - 2.0f, (0.0008f + 0.00004f * juce::jmin (rt60, 60.0f)) * sr);
        outScale = 1.0f / std::sqrt (1.0f + 0.22f * juce::jmin (rt60, 25.0f));
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        float x = 0.5f * (inL + inR);
        for (int i = 0; i < 6; ++i)  // series all-pass diffusion (bloom)
        {
            auto& b = ap[i];
            const float d = b[(size_t) apPos[i]];
            const float g = i < 4 ? 0.62f : 0.5f;
            const float y = -g * x + d;
            b[(size_t) apPos[i]] = x + g * y;
            if (++apPos[i] >= apLen[i]) apPos[i] = 0;
            x = y;
        }

        float o[8];
        for (int i = 0; i < 8; ++i)
        {
            lfo[i] += lfoInc[i]; if (lfo[i] >= 1.0f) lfo[i] -= 1.0f;
            const float m = modDepth * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * lfo[i]));
            float rp = (float) pos[i] - (float) len[i] - m;
            while (rp < 0) rp += (float) size[i];
            const int i0 = (int) rp % size[i];
            const int i1 = (i0 + 1) % size[i];
            const float f = rp - std::floor (rp);
            const float v = line[i][(size_t) i0] + f * (line[i][(size_t) i1] - line[i][(size_t) i0]);
            lp[i] += damp * (v - lp[i]);
            o[i] = lp[i] * gain[i];
        }
        float h[8];
        for (int i = 0; i < 8; ++i) h[i] = o[i];
        for (int st = 1; st < 8; st <<= 1)
            for (int i = 0; i < 8; i += st << 1)
                for (int j = i; j < i + st; ++j)
                {
                    const float a = h[j], b = h[j + st];
                    h[j] = a + b; h[j + st] = a - b;
                }
        const float norm = 0.35355339f;
        const float inG = infinite ? 0.02f : 1.0f;   // infinite: tail holds, new input barely enters
        for (int i = 0; i < 8; ++i)
        {
            float w = h[i] * norm + inG * (x * ((i & 1) ? 0.3f : -0.3f) + (i < 4 ? inL : inR) * 0.12f);
            line[i][(size_t) pos[i]] = w;
            if (++pos[i] >= size[i]) pos[i] = 0;
        }
        outL = (o[0] - o[2] + o[4] - o[6]) * 0.6f * outScale;
        outR = (o[1] - o[3] + o[5] - o[7]) * 0.6f * outScale;
    }

private:
    std::vector<float> line[8], ap[6];
    int len[8] {}, size[8] {}, pos[8] {}, apLen[6] {}, apPos[6] {}, maxMod = 8;
    float gain[8] {}, lp[8] {}, lfo[8] {}, lfoInc[8] {};
    float sr = 44100, damp = 0.3f, currentRT = -1, outScale = 1.0f, modDepth = 0.0f, tone = 0.5f;
    bool infinite = false;
};
