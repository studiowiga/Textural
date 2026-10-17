#include "GranularEngine.h"

namespace
{
    float fA = 0, fR = 0, sA = 0, sR = 0;   // onset follower coefficients (set in prepare)
    constexpr double kBufferSeconds = 12.0;
    constexpr double kLayerSeconds  = 2.5;
}

void GranularEngine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    bufLen = (int) (kBufferSeconds * sr);
    bufL.assign ((size_t) bufLen, 0.0f);
    bufR.assign ((size_t) bufLen, 0.0f);

    for (auto& l : layers)
    {
        l.L.assign ((size_t) (kLayerSeconds * sr), 0.0f);
        l.R.assign ((size_t) (kLayerSeconds * sr), 0.0f);
    }

    fA = 1.0f - std::exp (-1.0f / (0.0008f * (float) sr));
    fR = 1.0f - std::exp (-1.0f / (0.020f  * (float) sr));
    sA = 1.0f - std::exp (-1.0f / (0.050f  * (float) sr));
    sR = 1.0f - std::exp (-1.0f / (0.350f  * (float) sr));
    fbLpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 10000.0f / (float) sr);
    fbHpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 70.0f / (float) sr);
    reset();
}

void GranularEngine::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    for (auto& g : grains) g.active = false;
    for (auto& l : layers)
    {
        l.stage = Layer::Off; l.env = 0; l.recording = false; l.length = 0;
        std::fill (l.L.begin(), l.L.end(), 0.0f);
        std::fill (l.R.begin(), l.R.end(), 0.0f);
    }
    afRecording = afHasSegment = false;
    envFast = envSlow = 0; holdOff = 0;
    spawnCounter = 0;
    fbPrevL = fbPrevR = 0;
    fbEnv = 0;
    fbLpL = fbLpR = fbHpL = fbHpR = 0;
    writePos = 0;
    frozen = false;
    layersActive = 0;
}

float GranularEngine::grainRatio (const EngineParams& p)
{
    float semis = Map::pitchSemis (p.pitch);
    if (p.quantize) semis = Map::quantiseSemis (semis);

    // PITCH RANDOM: how much and how often grains jump to harmonically related intervals
    if (p.pitchRnd > 0.001f && rnd() < p.pitchRnd * 0.85f)
    {
        static const float intervals[] = { 12, -12, 7, -5, 19, 24, -24 };
        const int count = juce::jlimit (2, 7, 2 + (int) (p.pitchRnd * 5.99f));
        semis += intervals[(int) (rnd() * (float) count) % count];
    }
    semis = juce::jlimit (-36.0f, 36.0f, semis);
    return std::exp2 (semis / 12.0f);
}

void GranularEngine::spawnGrain (const EngineParams& p, int source, double sourceEnd, float jitter, float density)
{
    Grain* g = nullptr;
    for (auto& cand : grains) if (! cand.active) { g = &cand; break; }
    if (g == nullptr) return;

    const float sizeSec = Map::sizeSeconds (p.size, p.mode);
    const float lenS    = sizeSec * (float) sr * (1.0f + p.sizeRnd * brnd() * 0.85f);
    const int   len     = juce::jmax (64, (int) lenS);
    const float rate    = grainRatio (p);
    const double span   = (double) len * rate;

    const double end   = sourceEnd - (double) (rnd() * jitter);
    const double start = end - span;
    const bool   rev   = p.reverse && rnd() < Map::reverseProbability (p.reverseRnd);

    g->active = true;
    g->source = source;
    g->length = len;
    g->age    = 0;
    g->inc    = rev ? -rate : rate;
    g->pos    = rev ? end : start;

    const float norm = 1.0f / std::pow (juce::jmax (1.0f, 0.75f * density), 0.75f);
    if (p.stereo)
    {
        const float pan = brnd() * 0.9f;                       // random panning across the field
        const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        g->gL = norm * std::cos (a) * 1.41421356f;
        g->gR = norm * std::sin (a) * 1.41421356f;
    }
    else
    {
        g->gL = g->gR = norm;
    }
}

bool GranularEngine::detectOnset (float x)
{
    const float a = std::abs (x);
    envFast += (a > envFast ? fA : fR) * (a - envFast);
    envSlow += (a > envSlow ? sA : sR) * (a - envSlow);
    if (holdOff > 0) { --holdOff; return false; }
    if (envFast > 0.0035f && envFast > envSlow * 1.9f + 0.0008f)
    {
        holdOff = (int) (0.12 * sr);
        return true;
    }
    return false;
}

void GranularEngine::onsetDetected (const EngineParams& p)
{
    if (p.mode == Mode::AutoFreeze)
    {
        afRecording = true;
        afRecStart  = writePos;
        afRecCount  = 0;
        afRecLen    = (int) (juce::jlimit (0.05f, 2.0f, Map::timeSeconds (p.time, Mode::Delay)) * sr);
    }
    else if (p.mode == Mode::Layers)
    {
        if (p.freeze) return;                      // frozen layers ignore new notes

        Layer* slot = nullptr;
        for (auto& l : layers) if (l.stage == Layer::Off && ! l.recording) { slot = &l; break; }
        if (slot == nullptr)
        {
            for (auto& l : layers)                 // replace the oldest of the four
                if (! l.recording && (slot == nullptr || l.order < slot->order)) slot = &l;
        }
        if (slot == nullptr) return;

        if (slot->stage != Layer::Off) slot->stage = Layer::Release;
        const float sizeSec = Map::sizeSeconds (p.size, Mode::Layers);
        slot->recording = true;
        slot->recStart  = writePos - (int) (0.008 * sr);
        slot->recCount  = 0;
        slot->recLen    = (int) (juce::jlimit (0.25f, 2.4f, sizeSec * 2.2f) * sr);
        slot->order     = ++layerCounter;
    }
}

void GranularEngine::process (const float* inLp, const float* inRp, float* wetL, float* wetR,
                              int numSamples, const EngineParams& p)
{
    if (p.mode != lastMode)
    {
        afRecording = afHasSegment = false;
        for (auto& l : layers) { l.stage = Layer::Off; l.recording = false; l.env = 0; }
        for (auto& g : grains) if (g.source >= 0) g.active = false;
        lastMode = p.mode;
    }

    const float timeTarget   = Map::timeSeconds (p.time, Mode::Delay);
    const float fb           = p.mode == Mode::Layers ? 0.0f : Map::feedback (p.regen);
    const float fbDelay      = fb * (1.0f - p.regenMix);
    const float fbGrain      = fb * p.regenMix;
    const float density      = Map::overlapCount (p.overlap);
    const float fbComp       = std::pow (juce::jmax (1.0f, 0.75f * density), 0.35f);
    const float sizeSec      = Map::sizeSeconds (p.size, p.mode);
    const bool  unfreezeHeld = p.mode == Mode::AutoFreeze && p.freeze;

    const float attackInc    = 1.0f / (Map::timeSeconds (p.time, Mode::Layers) * (float) sr);
    const float decayCoef    = std::exp (-6.9f / (Map::layerDecaySeconds (p.regen) * (float) sr));
    const float releaseDec   = 1.0f / (0.025f * (float) sr);

    auto nextInterval = [&]
    {
        const double base = p.tapSpacing > 0.0f ? p.tapSpacing * sr : sizeSec * sr / density;
        return juce::jmax (0.002 * sr, base * (1.0 + p.overlapRnd * brnd() * 0.9));
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = p.inputMuted ? 0.0f : inLp[i];
        const float inR = p.inputMuted ? 0.0f : inRp[i];

        if (! p.inputMuted && detectOnset (0.5f * (inL + inR)))
            onsetDetected (p);

        timeSmooth += 0.0005f * (timeTarget - timeSmooth);

        //---------------------------------------------------------------- write
        bool writing = true;
        if (p.mode == Mode::Delay)           writing = ! p.freeze;
        else if (p.mode == Mode::AutoFreeze) writing = afRecording || unfreezeHeld || ! afHasSegment;

        if (writing)
        {
            float wl = inL, wr = inR;
            if (p.mode != Mode::Layers && fb > 0.0f)
            {
                const double rp = (double) writePos - (double) timeSmooth * sr;
                const float dl = hermite (bufL.data(), bufLen, rp);
                const float dr = hermite (bufR.data(), bufLen, rp);
                float fl = fbDelay * dl + fbGrain * fbPrevL;
                float fr = fbDelay * dr + fbGrain * fbPrevR;
                // tone of the regeneration path: gentle high cut + DC/rumble blocker,
                // so cascading octaves fade naturally like on hardware
                fbLpL += fbLpCoef * (fl - fbLpL);  fbLpR += fbLpCoef * (fr - fbLpR);
                fbHpL += fbHpCoef * (fbLpL - fbHpL); fbHpR += fbHpCoef * (fbLpR - fbHpR);
                fl = fbLpL - fbHpL;  fr = fbLpR - fbHpR;
                // feedback level control: lets REGEN self-sustain without running away
                const float pk = juce::jmax (std::abs (fl), std::abs (fr));
                fbEnv += (pk > fbEnv ? 0.004f : 0.00005f) * (pk - fbEnv);
                const float fbg = fbEnv > 0.35f ? 0.35f / fbEnv : 1.0f;
                wl += softClip (fl * fbg);
                wr += softClip (fr * fbg);
            }
            bufL[(size_t) writePos] = wl;
            bufR[(size_t) writePos] = wr;
            if (++writePos >= bufLen) writePos = 0;
        }

        //---------------------------------------------------- Auto Freeze state
        if (afRecording && ++afRecCount >= afRecLen)
        {
            afRecording  = false;
            afHasSegment = true;
            afSegStart   = afRecStart;
            afSegLen     = afRecLen;
        }

        //--------------------------------------------------------- spawn grains
        if (p.mode == Mode::Delay || unfreezeHeld)
        {
            if ((spawnCounter -= 1.0) <= 0.0)
            {
                const double end = (double) writePos - timeSmooth * sr * (1.0 + 0.9 * p.timeRnd * brnd());
                spawnGrain (p, -1, end, p.timeRnd * sizeSec * (float) sr * 0.5f, density);
                spawnCounter = nextInterval();
            }
        }
        else if (p.mode == Mode::AutoFreeze && afHasSegment)
        {
            if ((spawnCounter -= 1.0) <= 0.0)
            {
                spawnGrain (p, -1, (double) (afSegStart + afSegLen), p.timeRnd * (float) afSegLen, density);
                spawnCounter = nextInterval();
            }
        }
        else if (p.mode == Mode::Layers)
        {
            for (int li = 0; li < kLayers; ++li)
            {
                auto& l = layers[(size_t) li];

                if (l.recording && ++l.recCount >= l.recLen)
                {
                    for (auto& g : grains) if (g.source == li) g.active = false;
                    const int n = juce::jmin (l.recLen, (int) l.L.size());
                    for (int k = 0; k < n; ++k)
                    {
                        int idx = (l.recStart + k) % bufLen; if (idx < 0) idx += bufLen;
                        l.L[(size_t) k] = bufL[(size_t) idx];
                        l.R[(size_t) k] = bufR[(size_t) idx];
                    }
                    l.length = n;
                    l.recording = false;
                    l.stage = Layer::Attack;
                    l.env = 0;
                    l.spawnCounter = 0;
                }

                switch (l.stage)
                {
                    case Layer::Attack:
                        l.env += attackInc;
                        if (l.env >= 1.0f) { l.env = 1.0f; l.stage = Layer::Sustain; }
                        break;
                    case Layer::Sustain:
                        if (! p.freeze) { l.env *= decayCoef; if (l.env < 1.0e-4f) { l.env = 0; l.stage = Layer::Off; } }
                        break;
                    case Layer::Release:
                        l.env -= releaseDec;
                        if (l.env <= 0) { l.env = 0; l.stage = Layer::Off; }
                        break;
                    case Layer::Off: break;
                }

                if ((l.stage == Layer::Attack || l.stage == Layer::Sustain) && l.length > 0)
                {
                    if ((l.spawnCounter -= 1.0) <= 0.0)
                    {
                        spawnGrain (p, li, (double) l.length, 0.0f, density);
                        l.spawnCounter = nextInterval();
                    }
                }
            }
        }

        //--------------------------------------------------------- render grains
        float sumL = 0, sumR = 0;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            float w = window ((float) g.age / (float) g.length);
            float sL, sR;
            if (g.source < 0)
            {
                sL = hermite (bufL.data(), bufLen, g.pos);
                sR = hermite (bufR.data(), bufLen, g.pos);
            }
            else
            {
                auto& l = layers[(size_t) g.source];
                const int ll = (int) l.L.size();
                sL = hermite (l.L.data(), ll, g.pos);
                sR = hermite (l.R.data(), ll, g.pos);
                w *= l.env * 0.7f;                       // layer amplitude envelope
            }

            if (p.stereo) { sumL += sL * w * g.gL; sumR += sR * w * g.gR; }
            else          { const float m = 0.5f * (sL + sR) * w * g.gL; sumL += m; sumR += m; }

            g.pos += g.inc;
            if (++g.age >= g.length) g.active = false;
        }

        fbPrevL = sumL * fbComp;      // granular feedback with power-normalised density
        fbPrevR = sumR * fbComp;
        wetL[i] = sumL;
        wetR[i] = sumR;
    }

    bool fr = false;
    if (p.mode == Mode::Delay)           fr = p.freeze;
    else if (p.mode == Mode::AutoFreeze) fr = afHasSegment && ! afRecording && ! unfreezeHeld;
    else                                 fr = p.freeze;
    frozen = fr;

    int act = 0;
    for (auto& l : layers) if (l.stage != Layer::Off) ++act;
    layersActive = act;
}
