#include "GranularEngine.h"

namespace
{
    constexpr double kBufferSeconds = 12.0;
    constexpr double kLayerSeconds  = 2.6;
    constexpr int    kGuard         = 4;     // samples kept between read and write heads
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
    envFast = envSlow = 0; holdOff = 0; noiseFloor = 0.001f;
    spawnCounter = particleCounter = revCounter = 0; revNorm = 1.0f; revMix = 0.0f;
    fbPrevL = fbPrevR = fbEnv = 0;
    fbLpL = fbLpR = fbHpL = fbHpR = 0;
    wowPhase = 0; haveLast = false;
    writePos = 0;
    frozen = false;
    layersActive = 0;
}

GranularEngine::Grain* GranularEngine::freeGrain()
{
    for (auto& g : grains) if (! g.active) return &g;
    return nullptr;
}

float GranularEngine::effectiveDensity (const EngineParams& p) const
{
    // OVERLAP: 1..30 grains; never below 1.25 so successive grains always cross-fade
    return juce::jmax (1.25f, Map::overlapCount (p.overlap));
}

float GranularEngine::grainSemis (const EngineParams& p)
{
    float semis = Map::pitchSemis (p.pitch);
    if (p.quantize) semis = Map::quantiseSemis (semis);

    // PITCH RANDOM, first third: unison detune (up to +/-30 cents) - lush, chorus-like synth
    const float detune = juce::jmin (p.pitchRnd, 0.33f) / 0.33f;
    semis += brnd() * 0.30f * detune;
    // beyond: grains jump more and more often to harmonically related intervals
    const float jump = juce::jmax (0.0f, (p.pitchRnd - 0.33f) / 0.67f);
    if (jump > 0.001f && rnd() < jump * 0.8f)
    {
        static const float intervals[] = { 12, -12, 7, 19, -5, 24, 5 };
        const int count = juce::jlimit (2, 7, 2 + (int) (jump * 5.99f));
        semis += intervals[(int) (rnd() * (float) count) % count];
    }
    return juce::jlimit (-36.0f, 36.0f, semis);
}

//------------------------------------------------------------------------------
// WSOLA-style phase alignment: shift the new grain (within +/-12 ms) so that its
// fade-in matches the waveform of the grain it cross-fades with. Without it two
// grains of the same note can cancel during the cross-fade (audible drop-outs),
// especially in reverse. Returns the best shift (in source samples).
double GranularEngine::alignToPrevious (const Grain& fresh, int fadeLen, double minShift, double maxShift, float& corrOut)
{
    corrOut = 0.0f;
    // reference = what the grains already sounding will output during the fade-in
    int n = fadeLen;
    bool any = false;
    for (auto& g : grains)
        if (g.active && &g != &fresh && g.source == fresh.source && g.stream == fresh.stream && g.amp >= 1.0f)
        { any = true; n = juce::jmin (n, juce::jmax (fadeLen / 2, g.length - g.age)); }
    if (! any || n < 64) return 0.0;

    const float* bL = fresh.source < 0 ? bufL.data() : layers[(size_t) fresh.source].L.data();
    const int    bl = fresh.source < 0 ? bufLen      : (int) layers[(size_t) fresh.source].L.size();

    const int step = juce::jmax (3, n / 256);
    float ref[256], wnew[256];
    const int m = juce::jmin (256, n / step);
    for (int k = 0; k < m; ++k) { ref[k] = 0.0f; wnew[k] = window ((float) (k * step) / (float) fresh.length, fresh.taper, false); }
    for (auto& g : grains)
    {
        if (! g.active || &g == &fresh || g.source != fresh.source || g.stream != fresh.stream || g.amp < 1.0f) continue;
        for (int k = 0; k < m; ++k)
        {
            const int age = g.age + k * step;
            if (age >= g.length) break;
            ref[k] += window ((float) age / (float) g.length, g.taper, false)
                      * hermite (bL, bl, g.pos + g.inc * (double) (k * step));
        }
    }
    double refE = 0;
    for (int k = 0; k < m; ++k) refE += ref[k] * ref[k];
    if (refE < 1.0e-7) return 0.0;

    double best = 0.0; double bestC = -2.0;
    auto score = [&] (double sh)
    {
        double c = 0, e = 0;
        for (int k = 0; k < m; ++k)
        {
            const float v = wnew[k] * hermite (bL, bl, fresh.pos + sh + fresh.inc * (double) (k * step));
            c += v * ref[k]; e += v * v;
        }
        return e < 1.0e-9 ? -2.0 : c / std::sqrt (e * refE);
    };
    // coarse search, then refine around the best match
    const double coarse = juce::jmax (1.0, std::abs (fresh.inc)) * 4.0;
    for (double sh = minShift; sh <= maxShift; sh += coarse)
    {
        const double nc = score (sh);
        if (nc > bestC) { bestC = nc; best = sh; }
    }
    const double fine = juce::jmax (1.0, std::abs (fresh.inc));
    const double c0 = best;
    for (double sh = juce::jmax (minShift, c0 - coarse); sh <= juce::jmin (maxShift, c0 + coarse); sh += fine * 0.5)
    {
        const double nc = score (sh);
        if (nc > bestC) { bestC = nc; best = sh; }
    }
    corrOut = (float) bestC;
    return best;
}

//------------------------------------------------------------------------------
// regionStart..regionEnd : readable audio (absolute positions, may exceed bufLen;
//                          hermite wraps). anchor = preferred grain end.
// scatter                : 0..1 random spread of the grain position in the region.
void GranularEngine::spawnGrain (const EngineParams& p, int source, double regionStart, double regionEnd,
                                 double anchor, float scatter)
{
    Grain* g = freeGrain();
    if (g == nullptr) return;

    const float density = effectiveDensity (p);
    const float sizeSec = Map::sizeSeconds (p.size, p.mode);
    int len = juce::jmax (96, (int) (sizeSec * (float) sr * (1.0f + p.sizeRnd * brnd() * 0.85f)));
    float rate = std::exp2 (grainSemis (p) / 12.0f);
    if (source >= 0) rate *= std::exp2 (layers[(size_t) source].drift / 1200.0f);

    double span = (double) len * rate;
    const double regionLen = regionEnd - regionStart;
    if (span > regionLen && regionLen > 256.0)          // keep the grain inside readable audio
    {
        len  = juce::jmax (96, (int) (regionLen / rate));
        span = (double) len * rate;
    }

    double end = anchor - (double) (rnd() * scatter) * juce::jmax (0.0, regionLen - span);
    end = juce::jlimit (regionStart + span, regionEnd, end);
    double start = end - span;

    // GLITCH: high TIME RANDOM makes some grains re-fire the previous slice (stutter)
    if (source < 0 && haveLast && p.timeRnd > 0.45f && rnd() < (p.timeRnd - 0.45f) * 0.9f
        && lastStart >= regionStart && lastStart + (double) lastLen * lastRate <= regionEnd)
    {
        start = lastStart;
        len   = juce::jmax (96, (int) ((float) lastLen * (rnd() < 0.5f ? 0.5f : 1.0f)));
        rate  = lastRate;
        end   = start + (double) len * rate;
    }
    lastStart = start; lastLen = len; lastRate = rate; haveLast = source < 0;

    // with the REVERSE echo stream running (Delay / Auto Freeze), these grains stay forward;
    // in Layers mode REVERSE flips grains with the REVERSE RANDOM probability
    const bool rev = p.reverse && ! reverseStreamActive (p) && rnd() < Map::reverseProbability (p.reverseRnd);

    g->active       = true;
    g->stream       = 0;
    g->source       = source;
    g->length       = len;
    g->age          = 0;
    g->inc          = rev ? -rate : rate;
    g->pos          = rev ? end : start;
    g->taper        = juce::jlimit (0.2f, 0.5f, 1.0f - 1.0f / density);
    g->equalPower   = rev;
    g->amp          = 1.0f;

    // sparse clouds (where each cross-fade is audible): align the new grain's phase
    if (density < 4.5f)
    {
        const int fadeLen = (int) ((float) len * g->taper);
        const double maxS = 0.012 * sr;
        // keep the grain inside the readable region whatever the shift
        const double lo = rev ? juce::jmax (-maxS, (regionStart + span) - end) : juce::jmax (-maxS, regionStart - start);
        const double hi = rev ? juce::jmin ( maxS, regionEnd - end)            : juce::jmin ( maxS, regionEnd - end);
        if (hi > lo)
        {
            float corr = 0.0f;
            const double sh = alignToPrevious (*g, fadeLen, lo, hi, corr);
            g->pos += sh;
            g->equalPower = corr < 0.5f;     // well aligned -> amplitude-complementary fade
        }
    }
    g->stereoOffset = p.stereo ? (double) (rnd() * 0.003f * (float) sr) : 0.0;   // only further in the past

    if (p.stereo)
    {
        const float pan = brnd() * 0.85f;
        const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        g->gL = std::cos (a) * 1.41421356f;
        g->gR = std::sin (a) * 1.41421356f;
    }
    else g->gL = g->gR = 1.0f;
}

// MIX above 75 %: a cloud of tiny "particle" grains (10-45 ms) scattered in time
// and across the stereo field; sparkle (octave/twelfth) follows PITCH RANDOM,
// direction follows REVERSE.
void GranularEngine::spawnParticle (const EngineParams& p)
{
    Grain* g = freeGrain();
    if (g == nullptr) return;

    int source = -1;
    double lo, hi;
    if (p.mode == Mode::Layers)
    {
        int cand[kLayers], n = 0;
        for (int i = 0; i < kLayers; ++i)
            if (layers[(size_t) i].stage != Layer::Off && layers[(size_t) i].length > 0) cand[n++] = i;
        if (n == 0) return;
        source = cand[(int) (rnd() * (float) n) % n];
        lo = 0; hi = (double) layers[(size_t) source].length;
    }
    else if (p.mode == Mode::AutoFreeze && afHasSegment)
    {
        lo = afSegStart; hi = (double) afSegStart + afSegLen;
    }
    else
    {
        hi = (double) writePos - kGuard; lo = hi - 1.6 * sr;
    }

    const int len = (int) ((0.010f + 0.035f * rnd()) * (float) sr);
    float semis = Map::pitchSemis (p.pitch);
    if (p.quantize) semis = Map::quantiseSemis (semis);
    const float r = rnd() / juce::jmax (0.001f, juce::jmin (1.0f, p.pitchRnd * 3.0f));
    if (r < 0.30f) semis += 12.0f; else if (r < 0.42f) semis += 19.0f; else if (r < 0.48f) semis += 24.0f;
    const double rate = std::exp2 (juce::jlimit (-36.0f, 36.0f, semis) / 12.0f);
    const double span = (double) len * rate;
    if (hi - lo < span + 8) return;
    const double end = lo + span + (double) rnd() * (hi - lo - span);
    const bool rev = p.reverse ? rnd() < Map::reverseProbability (p.reverseRnd) : rnd() < 0.2f;

    g->active = true;
    g->source = source;
    g->length = len;
    g->age    = 0;
    g->inc    = rev ? -rate : rate;
    g->pos    = rev ? end : end - span;
    g->taper  = 0.5f;
    g->equalPower = false;
    g->stereoOffset = 0;
    const float pan = brnd();
    const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    g->gL = std::cos (a) * 1.41421356f;
    g->gR = std::sin (a) * 1.41421356f;
    g->amp = 0.5f;
    g->stream = 2;
}

//------------------------------------------------------------------------------
// REVERSE: the recorded guitar played backwards, phrase by phrase (classic reverse
// echo). Each chunk (length = SIZE, at least 0.25 s) is read backwards from the
// newest audio (TIME behind the head); chunks follow each other with a 20 %
// phase-aligned cross-fade, so the reversed sound is continuous and click-free.
void GranularEngine::spawnReverseChunk (const EngineParams& p, double regionStart, double regionEnd,
                                        double anchor, float scatter)
{
    Grain* g = freeGrain();
    if (g == nullptr) return;

    float semis = Map::pitchSemis (p.pitch);
    if (p.quantize) semis = Map::quantiseSemis (semis);
    const double rate = std::exp2 (semis / 12.0f);
    int len = (int) (juce::jlimit (0.25f, 2.0f, Map::sizeSeconds (p.size, p.mode)) * (float) sr);
    double span = (double) len * rate;
    const double regionLen = regionEnd - regionStart;
    if (span > regionLen && regionLen > 512.0) { len = (int) (regionLen / rate); span = (double) len * rate; }

    double end = anchor - (double) (rnd() * scatter) * juce::jmax (0.0, regionLen - span);
    end = juce::jlimit (regionStart + span, regionEnd, end);

    g->active = true;
    g->source = -1;
    g->stream = 1;
    g->length = len;
    g->age    = 0;
    g->inc    = -rate;
    g->pos    = end;
    g->taper  = 0.2f;
    g->equalPower = false;
    g->amp    = 1.0f;
    g->stereoOffset = p.stereo ? (double) (rnd() * 0.002f * (float) sr) : 0.0;
    if (p.stereo)
    {
        const float pan = brnd() * 0.35f;                      // reversed phrases stay fairly central
        const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        g->gL = std::cos (a) * 1.41421356f;
        g->gR = std::sin (a) * 1.41421356f;
    }
    else g->gL = g->gR = 1.0f;

    // phase-align with the chunk that is fading out
    const int fadeLen = (int) ((float) len * g->taper);
    const double maxS = 0.012 * sr;
    const double lo = juce::jmax (-maxS, (regionStart + span) - end);
    const double hi = juce::jmin ( maxS, regionEnd - end);
    if (hi > lo)
    {
        float corr = 0.0f;
        g->pos += alignToPrevious (*g, fadeLen, lo, hi, corr);
    }
}

bool GranularEngine::detectOnset (float x)
{
    const float a = std::abs (x);
    envFast += (a > envFast ? fA : fR) * (a - envFast);
    envSlow += (a > envSlow ? sA : sR) * (a - envSlow);
    noiseFloor += (envSlow < noiseFloor ? 0.002f : 0.000002f) * (envSlow - noiseFloor);
    if (holdOff > 0) { --holdOff; return false; }
    if (envFast > juce::jmax (0.0025f, noiseFloor * 4.0f) && envFast > envSlow * 1.7f + 0.0006f)
    {
        holdOff = (int) (holdSeconds * sr);
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
        if (p.freeze) return;
        Layer* slot = nullptr;
        for (auto& l : layers) if (l.stage == Layer::Off && ! l.recording) { slot = &l; break; }
        if (slot == nullptr)
            for (auto& l : layers)
                if (! l.recording && (slot == nullptr || l.order < slot->order)) slot = &l;
        if (slot == nullptr) return;

        if (slot->stage != Layer::Off) slot->stage = Layer::Release;
        const float sizeSec = Map::sizeSeconds (p.size, Mode::Layers);
        slot->recording = true;
        slot->recStart  = writePos - (int) (0.002 * sr);
        slot->recCount  = 0;
        slot->recLen    = (int) (juce::jlimit (0.6f, 2.5f, 0.45f + sizeSec * 3.0f) * sr);
        slot->order     = ++layerCounter;
        slot->drift     = brnd() * 6.0f;                    // +/- 6 cents per voice
    }
}

//==============================================================================
void GranularEngine::process (const float* inLp, const float* inRp, const float* recLp, const float* recRp,
                              float* wetL, float* wetR, int numSamples, const EngineParams& p)
{
    if (p.mode != lastMode)
    {
        afRecording = afHasSegment = false;
        for (auto& l : layers) { l.stage = Layer::Off; l.recording = false; l.env = 0; }
        for (auto& g : grains) g.active = false;
        lastMode = p.mode;
    }

    holdSeconds = p.mode == Mode::Layers ? 0.22f : 0.12f;
    const float timeTarget   = Map::timeSeconds (p.time, Mode::Delay);
    const float fb           = p.mode == Mode::Layers ? 0.0f : Map::feedback (p.regen);
    const float fbDelay      = fb * (1.0f - p.regenMix);
    const float fbGrain      = fb * p.regenMix;
    const float density      = effectiveDensity (p);
    const float sizeSec      = Map::sizeSeconds (p.size, p.mode);
    const bool  unfreezeHeld = p.mode == Mode::AutoFreeze && p.freeze;

    // output normalisation for overlapping, mostly uncorrelated grains
    const float taper   = juce::jlimit (0.2f, 0.5f, 1.0f - 1.0f / density);
    const bool  sparse  = density < 4.5f && p.mode != Mode::Layers;
    const float targetN = 1.0f / std::sqrt (juce::jmax (1.0f, density * (1.0f - taper) * 0.85f));
    const float fbComp  = std::pow (juce::jmax (1.0f, density * (1.0f - taper)), 0.3f);

    // repeats: darker below noon (analog echo), thinner above (lo-fi)
    const float toneLP = p.cutoff < 0.5f ? 1500.0f * std::pow (7000.0f / 1500.0f, p.cutoff / 0.5f) : 7000.0f;   // sweet, non-piercing cascades
    const float toneHP = p.cutoff > 0.5f ? 70.0f * std::pow (900.0f / 70.0f, (p.cutoff - 0.5f) / 0.5f) : 70.0f;
    const float lpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneLP / (float) sr);
    const float hpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHP / (float) sr);
    const float particles = juce::jlimit (0.0f, 1.0f, (p.mix - 0.75f) / 0.25f);

    const float attackInc  = 1.0f / (Map::timeSeconds (p.time, Mode::Layers) * (float) sr);
    const float decayCoef  = std::exp (-6.9f / (Map::layerDecaySeconds (p.regen) * (float) sr));
    const float releaseDec = 1.0f / (0.030f * (float) sr);

    auto nextInterval = [&] (double grainLen)
    {
        const double base = p.tapSpacing > 0.0f ? p.tapSpacing * sr : grainLen / density;
        return juce::jmax (0.002 * sr, base * (1.0 + p.overlapRnd * brnd() * 0.9));
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = p.inputMuted ? 0.0f : inLp[i];
        const float inR = p.inputMuted ? 0.0f : inRp[i];
        const float rcL = p.inputMuted ? 0.0f : recLp[i];
        const float rcR = p.inputMuted ? 0.0f : recRp[i];

        if (! p.inputMuted && detectOnset (0.5f * (inL + inR)))
            onsetDetected (p);

        timeSmooth += 0.0004f * (timeTarget - timeSmooth);
        normGain   += 0.0005f * (targetN - normGain);

        //--------------------------------------------------------------- record
        bool writing = true;
        if (p.mode == Mode::Delay)           writing = ! p.freeze;
        else if (p.mode == Mode::AutoFreeze) writing = afRecording || unfreezeHeld || ! afHasSegment;

        if (writing)
        {
            float wl = rcL, wr = rcR;
            if (p.mode != Mode::Layers && fb > 0.0f)
            {
                wowPhase += 0.55f / (float) sr; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
                const double wow = (double) (p.vibDepth * 0.0025f * (float) sr
                                             * (0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * wowPhase)));
                const double rp = (double) writePos - (double) timeSmooth * sr - wow;
                float dl = hermite (bufL.data(), bufLen, rp);
                float dr = hermite (bufR.data(), bufLen, rp);
                if (p.stereo) std::swap (dl, dr);                        // ping-pong
                float fl = fbDelay * dl + fbGrain * fbPrevL;
                float fr = fbDelay * dr + fbGrain * fbPrevR;
                fbLpL += lpC * (fl - fbLpL);  fbLpR += lpC * (fr - fbLpR);
                fbHpL += hpC * (fbLpL - fbHpL); fbHpR += hpC * (fbLpR - fbHpR);
                fl = fbLpL - fbHpL;  fr = fbLpR - fbHpR;
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

        const double head = (double) writePos;

        //---------------------------------------------------- Auto Freeze state
        if (afRecording && ++afRecCount >= afRecLen)
        {
            afRecording  = false;
            afHasSegment = true;
            afSegStart   = afRecStart;
            afSegLen     = afRecLen;
        }

        //------------------------------------------------------------ sow grains
        if (p.mode == Mode::Delay || unfreezeHeld)
        {
            if ((spawnCounter -= 1.0) <= 0.0)
            {
                // grain ends TIME behind the write head (randomised by TIME RANDOM);
                // it only reads audio already recorded -> never crosses the head
                const double regionEnd   = head - kGuard - 0.004 * sr;
                const double regionStart = head - (double) bufLen + 0.1 * sr;
                const double anchor      = regionEnd - (double) timeSmooth * sr * (1.0 + 0.9 * p.timeRnd * brnd());
                spawnGrain (p, -1, regionStart, regionEnd, juce::jmin (anchor, regionEnd), 0.0f);
                spawnCounter = nextInterval (sizeSec * sr);
            }
        }
        else if (p.mode == Mode::AutoFreeze && afHasSegment)
        {
            if ((spawnCounter -= 1.0) <= 0.0)
            {
                double s0 = (double) afSegStart, s1 = (double) afSegStart + afSegLen;
                // short recordings: grains may look a little into older audio ("time travel")
                const double needed = sizeSec * sr * 1.2;
                if (s1 - s0 < needed) s0 = s1 - needed;
                spawnGrain (p, -1, s0, s1, s1, 0.35f + 0.65f * p.timeRnd);
                spawnCounter = nextInterval (sizeSec * sr);
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
                    l.length = n; l.recording = false;
                    l.stage = Layer::Attack; l.env = 0; l.spawnCounter = 0;
                }

                switch (l.stage)
                {
                    case Layer::Attack:  l.env += attackInc; if (l.env >= 1.0f) { l.env = 1.0f; l.stage = Layer::Sustain; } break;
                    case Layer::Sustain: if (! p.freeze) { l.env *= decayCoef; if (l.env < 1.0e-4f) { l.env = 0; l.stage = Layer::Off; } } break;
                    case Layer::Release: l.env -= releaseDec; if (l.env <= 0) { l.env = 0; l.stage = Layer::Off; } break;
                    case Layer::Off: break;
                }

                if ((l.stage == Layer::Attack || l.stage == Layer::Sustain) && l.length > 0
                    && (l.spawnCounter -= 1.0) <= 0.0)
                {
                    // grains scattered over the body of the note (skip the pick transient)
                    spawnGrain (p, li, 0.03 * sr, (double) l.length, (double) l.length, 1.0f);
                    l.spawnCounter = nextInterval (sizeSec * sr);
                }
            }
        }

        //------------------------------------------------- REVERSE echo stream
        if (reverseStreamActive (p) && (revCounter -= 1.0) <= 0.0)
        {
            const double chunk = juce::jlimit (0.25, 2.0, (double) Map::sizeSeconds (p.size, p.mode)) * sr;
            if (p.mode == Mode::Delay || unfreezeHeld)
            {
                const double regionEnd   = head - kGuard - 0.004 * sr;
                const double regionStart = head - (double) bufLen + 0.1 * sr;
                spawnReverseChunk (p, regionStart, regionEnd, regionEnd - (double) timeSmooth * sr + 0.05 * sr, 0.0f);
                revCounter = chunk * 0.8;
            }
            else if (p.mode == Mode::AutoFreeze && afHasSegment)
            {
                double s0 = (double) afSegStart, s1 = (double) afSegStart + afSegLen;
                if (s1 - s0 < chunk * 1.1) s0 = s1 - chunk * 1.1;
                spawnReverseChunk (p, s0, s1, s1, 0.3f * p.timeRnd);
                revCounter = chunk * 0.8;
            }
            else revCounter = 0.01 * sr;
        }

        if (particles > 0.0f && (particleCounter -= 1.0) <= 0.0)
        {
            spawnParticle (p);
            particleCounter = sr / (4.0 + 46.0 * particles) * (0.3 + 1.4 * rnd());
        }

        //--------------------------------------------------------------- render
        float sumL = 0, sumR = 0, wsum = 0;          // grains + particles
        float rvL = 0, rvR = 0, rvW = 0;             // REVERSE echo stream
        for (auto& g : grains)
        {
            if (! g.active) continue;
            float w = window ((float) g.age / (float) g.length, g.taper, g.equalPower && ! sparse) * g.amp;
            float smpL, smpR;
            if (g.source < 0)
            {
                smpL = hermite (bufL.data(), bufLen, g.pos);
                smpR = hermite (bufR.data(), bufLen, g.pos - g.stereoOffset);
            }
            else
            {
                auto& l = layers[(size_t) g.source];
                const int ll = (int) l.L.size();
                smpL = hermite (l.L.data(), ll, g.pos);
                smpR = hermite (l.R.data(), ll, g.pos - g.stereoOffset);
                w *= l.env * 1.15f;
            }
            float oL, oR;
            if (p.stereo) { oL = smpL * w * g.gL; oR = smpR * w * g.gR; }
            else          { oL = oR = 0.5f * (smpL + smpR) * w; }

            if (g.stream == 1) { rvL += oL; rvR += oR; rvW += w; }
            else
            {
                sumL += oL; sumR += oR;
                if (g.stream == 0) wsum += w;
            }

            g.pos += g.inc;
            if (++g.age >= g.length) g.active = false;
        }

        if (sparse)   // phase-aligned overlap-add: normalise by the window sum -> constant level
        {
            olaNorm += 0.02f * (1.0f / juce::jmax (0.35f, wsum) - olaNorm);
            sumL *= olaNorm; sumR *= olaNorm;
        }
        else
        {
            // frozen washes sit lower than the live note: make-up gain in Auto Freeze
            const float g = normGain * (p.mode == Mode::AutoFreeze ? 1.5f : 1.0f);
            sumL *= g; sumR *= g;
        }

        // REVERSE: blend the backwards guitar with the grains (REVERSE RANDOM = 10..100 %)
        const float revTarget = reverseStreamActive (p) ? Map::reverseProbability (p.reverseRnd) : 0.0f;
        revMix  += 0.0004f * (revTarget - revMix);
        revNorm += 0.02f * (1.0f / juce::jmax (0.35f, rvW) - revNorm);
        if (revMix > 0.0005f || rvW > 0.0f)
        {
            const float gR = std::sqrt (juce::jlimit (0.0f, 1.0f, revMix));
            const float gN = std::sqrt (juce::jlimit (0.0f, 1.0f, 1.0f - revMix));
            sumL = sumL * gN + rvL * revNorm * gR;
            sumR = sumR * gN + rvR * revNorm * gR;
        }
        fbPrevL = sumL * fbComp;
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
