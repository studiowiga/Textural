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
//  Small delay-line helpers
//==============================================================================
struct DelayLine
{
    std::vector<float> b; int w = 0, n = 1;
    void init (int len) { n = juce::jmax (4, len); b.assign ((size_t) n, 0.0f); w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); }
    inline void push (float x) { b[(size_t) w] = x; if (++w >= n) w = 0; }
    inline float tap (int d) const { int i = w - 1 - d; while (i < 0) i += n; return b[(size_t) i]; }
    inline float tapf (float d) const
    {
        const int i0 = (int) d; const float f = d - (float) i0;
        return tap (i0) + f * (tap (i0 + 1) - tap (i0));
    }
};

// Schroeder all-pass with optional modulated (fractional) delay
struct AllPass
{
    DelayLine d; float len = 1, g = 0.5f;
    void init (float lengthSamples, float gain, int extra = 0) { len = lengthSamples; g = gain; d.init ((int) lengthSamples + extra + 4); }
    void clear() { d.clear(); }
    inline float process (float x, float mod = 0.0f)
    {
        const float delayed = d.tapf (juce::jmax (1.0f, len + mod) - 1.0f);
        const float v = x + g * delayed;
        d.push (v);
        return delayed - g * v;
    }
};

//==============================================================================
//  Stereo diffuser (all-pass network, "reverb without a tail", like Clouds):
//  smears grain edges into a smooth, blurred texture.
//==============================================================================
class Diffuser
{
public:
    void prepare (double sampleRate)
    {
        const float k = (float) sampleRate / 1000.0f;
        static const float msL[4] = { 3.13f, 7.37f, 11.71f, 17.29f };
        static const float msR[4] = { 3.71f, 6.83f, 12.97f, 16.13f };
        for (int i = 0; i < 4; ++i) { apL[i].init (msL[i] * k, 0.62f); apR[i].init (msR[i] * k, 0.62f); }
    }
    void reset() { for (auto& a : apL) a.clear(); for (auto& a : apR) a.clear(); }
    void process (float& l, float& r, float amount)
    {
        if (amount <= 0.001f) return;
        float a = l, b = r;
        for (auto& ap : apL) a = ap.process (a);
        for (auto& ap : apR) b = ap.process (b);
        l += amount * (a - l);
        r += amount * (b - r);
    }
private:
    AllPass apL[4], apR[4];
};

//==============================================================================
//  Plate reverb after Jon Dattorro, "Effect Design Part 1" (JAES 1997):
//  input band-limiting + 4 input diffusers + figure-of-eight modulated tank.
//  Modulation keeps long tails lush and free of metallic ringing; the decay
//  goes from short rooms to an infinite "hold" at the top of the range.
//==============================================================================
class FDNReverb   // (kept name for the processor) -> Dattorro plate
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        k = sr / 29761.0f;
        preDelay.init ((int) (0.012f * sr) + 4);
        inAP[0].init (142 * k, 0.75f); inAP[1].init (107 * k, 0.75f);
        inAP[2].init (379 * k, 0.625f); inAP[3].init (277 * k, 0.625f);
        const int ex = (int) (24 * k) + 4;
        tankAP1[0].init (672 * k, -0.70f, ex);  tankAP1[1].init (908 * k, -0.70f, ex);
        tankDel1[0].init ((int) (4453 * k) + 8); tankDel1[1].init ((int) (4217 * k) + 8);
        tankAP2[0].init (1800 * k, 0.5f);       tankAP2[1].init (2656 * k, 0.5f);
        tankDel2[0].init ((int) (3720 * k) + 8); tankDel2[1].init ((int) (3163 * k) + 8);
        len1[0] = 4453 * k; len1[1] = 4217 * k; len2[0] = 3720 * k; len2[1] = 3163 * k;
        loopSeconds = (672 + 4453 + 1800 + 3720) / 29761.0f;
        reset();
        currentRT = -1;
        setDecay (3.0f);
    }

    void reset()
    {
        preDelay.clear(); for (auto& a : inAP) a.clear();
        for (int i = 0; i < 2; ++i) { tankAP1[i].clear(); tankAP2[i].clear(); tankDel1[i].clear(); tankDel2[i].clear(); damp[i] = 0; }
        bw = 0; fb[0] = fb[1] = 0; lfo = 0;
    }

    void setTone (float t) { tone = juce::jlimit (0.0f, 1.0f, t); }

    void setDecay (float rt60)
    {
        if (std::abs (rt60 - currentRT) < 0.01f) return;
        currentRT = rt60;
        infinite = rt60 > 100.0f;
        decay = infinite ? 0.9999f : juce::jlimit (0.05f, 0.995f, std::pow (10.0f, -3.0f * loopSeconds / rt60));
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        // tone: dark plate below noon, bright above
        const float dampCoef = juce::jlimit (0.02f, 0.7f, (infinite ? 0.15f : 0.25f) - 0.4f * (tone - 0.5f));
        const float bwCoef   = juce::jlimit (0.35f, 0.9995f, 0.72f + 0.5f * (tone - 0.5f));

        float x = 0.5f * (inL + inR) * (infinite ? 0.0f : 1.0f);
        preDelay.push (x);
        x = preDelay.tap ((int) (0.010f * sr));
        bw += bwCoef * (x - bw);
        x = bw;
        for (auto& a : inAP) x = a.process (x);

        lfo += 0.9f / sr; if (lfo >= 1.0f) lfo -= 1.0f;
        const float exc = 12.0f * k;
        const float m0 = exc * std::sin (juce::MathConstants<float>::twoPi * lfo);
        const float m1 = exc * std::sin (juce::MathConstants<float>::twoPi * (lfo + 0.25f));

        float a = x + fb[1];
        float b = x + fb[0];
        a = tankAP1[0].process (a, m0);  tankDel1[0].push (a);
        b = tankAP1[1].process (b, m1);  tankDel1[1].push (b);
        float da = tankDel1[0].tapf (len1[0] - 1.0f);
        float db = tankDel1[1].tapf (len1[1] - 1.0f);
        damp[0] += (1.0f - dampCoef) * (da - damp[0]);
        damp[1] += (1.0f - dampCoef) * (db - damp[1]);
        da = tankAP2[0].process (damp[0] * decay);
        db = tankAP2[1].process (damp[1] * decay);
        tankDel2[0].push (da); tankDel2[1].push (db);
        fb[0] = tankDel2[0].tapf (len2[0] - 1.0f) * decay;
        fb[1] = tankDel2[1].tapf (len2[1] - 1.0f) * decay;

        auto T = [this] (const DelayLine& d, int s) { return d.tap ((int) ((float) s * k)); };
        float l = T (tankDel1[1], 266) + T (tankDel1[1], 2974) - T (tankAP2[1].d, 1913) + T (tankDel2[1], 1996)
                - T (tankDel1[0], 1990) - T (tankAP2[0].d, 187) - T (tankDel2[0], 1066);
        float r = T (tankDel1[0], 353) + T (tankDel1[0], 3627) - T (tankAP2[0].d, 1228) + T (tankDel2[0], 2673)
                - T (tankDel1[1], 2111) - T (tankAP2[1].d, 335) - T (tankDel2[1], 121);
        outL = l * 0.6f;
        outR = r * 0.6f;
    }

private:
    float sr = 44100, k = 1, decay = 0.5f, tone = 0.5f, bw = 0, lfo = 0, currentRT = -1, loopSeconds = 0.36f;
    float fb[2] {}, damp[2] {}, len1[2] {}, len2[2] {};
    bool infinite = false;
    DelayLine preDelay, tankDel1[2], tankDel2[2];
    AllPass inAP[4], tankAP1[2], tankAP2[2];
};
