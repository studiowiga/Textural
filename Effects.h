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
        for (auto& c : st) { c[0].set (fc, sr, 0.5412f); c[1].set (fc, sr, 1.3066f); } // Butterworth 4th order
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
// 8-line feedback delay network with input diffusion and damping
class FDNReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        static const float ms[8] = { 31.7f, 37.9f, 43.3f, 49.1f, 56.3f, 63.7f, 71.9f, 79.3f };
        for (int i = 0; i < 8; ++i)
        {
            len[i] = (int) (ms[i] * 0.001f * sr);
            line[i].assign ((size_t) len[i] + 1, 0.0f);
            pos[i] = 0;
            lp[i] = 0;
        }
        static const float apMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int i = 0; i < 4; ++i)
        {
            apLen[i] = (int) (apMs[i] * 0.001f * sr);
            ap[i].assign ((size_t) apLen[i] + 1, 0.0f);
            apPos[i] = 0;
        }
        setDecay (3.0f);
    }

    void reset()
    {
        for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& a : ap)   std::fill (a.begin(), a.end(), 0.0f);
        for (auto& v : lp) v = 0;
    }

    void setDecay (float rt60)
    {
        if (std::abs (rt60 - currentRT) < 0.01f) return;
        currentRT = rt60;
        for (int i = 0; i < 8; ++i)
            gain[i] = std::pow (10.0f, -3.0f * (float) len[i] / (rt60 * sr));
        damp = juce::jlimit (0.05f, 0.6f, 0.45f - 0.02f * rt60);
        outScale = 1.0f / std::sqrt (1.0f + 0.25f * rt60);
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        float x = 0.5f * (inL + inR);
        for (int i = 0; i < 4; ++i)  // series all-pass diffusion
        {
            auto& b = ap[i];
            const float d = b[(size_t) apPos[i]];
            const float y = -0.6f * x + d;
            b[(size_t) apPos[i]] = x + 0.6f * y;
            if (++apPos[i] >= apLen[i]) apPos[i] = 0;
            x = y;
        }

        float o[8];
        for (int i = 0; i < 8; ++i)
        {
            float v = line[i][(size_t) pos[i]];
            lp[i] += damp * (v - lp[i]);   // simple damping one-pole
            o[i] = lp[i] * gain[i];
        }
        // Hadamard 8x8 (fast transform) - energy preserving mixing
        float h[8];
        for (int i = 0; i < 8; ++i) h[i] = o[i];
        for (int s = 1; s < 8; s <<= 1)
            for (int i = 0; i < 8; i += s << 1)
                for (int j = i; j < i + s; ++j)
                {
                    const float a = h[j], b = h[j + s];
                    h[j] = a + b; h[j + s] = a - b;
                }
        const float norm = 0.35355339f;
        for (int i = 0; i < 8; ++i)
        {
            line[i][(size_t) pos[i]] = h[i] * norm + x * ((i & 1) ? 0.3f : -0.3f) + (i < 4 ? inL : inR) * 0.12f;
            if (++pos[i] >= len[i]) pos[i] = 0;
        }
        outL = (o[0] - o[2] + o[4] - o[6]) * 0.6f * outScale;
        outR = (o[1] - o[3] + o[5] - o[7]) * 0.6f * outScale;
    }

private:
    std::vector<float> line[8], ap[4];
    int len[8] {}, pos[8] {}, apLen[4] {}, apPos[4] {};
    float gain[8] {}, lp[8] {};
    float sr = 44100, damp = 0.3f, currentRT = -1, outScale = 1.0f;
};
