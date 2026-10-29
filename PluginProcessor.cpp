#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout TexturalAudioProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };

    auto knob = [&] (const String& id, const String& name, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    };
    auto toggle = [&] (const String& id, const String& name, bool def)
    {
        l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    knob (PID::time,    "Time",    0.50f);
    knob (PID::size,    "Size",    0.40f);
    knob (PID::overlap, "Overlap", 0.45f);
    knob (PID::regen,   "Regen",   0.30f);
    knob (PID::pitch,   "Pitch",   0.50f);
    knob (PID::mix,     "Mix",     0.50f);
    toggle (PID::reverse, "Reverse", false);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::mode, 1 }, "Mode",
                                                   StringArray { "Delay", "Auto Freeze", "Layers" }, 0));
    toggle (PID::bypass, "Bypass", false);
    toggle (PID::freeze, "Freeze (hold)", false);

    knob (PID::timeRnd,    "Time Random",    0.15f);
    knob (PID::sizeRnd,    "Size Random",    0.15f);
    knob (PID::overlapRnd, "Overlap Random", 0.15f);
    knob (PID::regenMix,   "Regen Mix",      0.50f);
    knob (PID::pitchRnd,   "Pitch Random",   0.00f);
    knob (PID::reverseRnd, "Reverse Random", 0.50f);

    knob (PID::reverbMix,   "Reverb Mix",     0.20f);
    knob (PID::reverbDecay, "Reverb Decay",   0.50f);
    knob (PID::cutoff,      "Filter Cutoff",  0.50f);
    toggle (PID::quantize,  "Quantize Pitch", false);
    knob (PID::vibRate,     "Vibrato Rate",   0.35f);
    knob (PID::vibDepth,    "Vibrato Depth",  0.00f);

    toggle (PID::presetMute, "Preset Scroll Mute", false);
    toggle (PID::trails,     "Trails", false);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::outputMode, 1 }, "Output Mode",
                                                   StringArray { "Mono", "Wet/Dry", "Stereo" }, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::fsHoldMode, 1 }, "Footswitch Hold Mode",
                                                   StringArray { "Preset Scroll", "Freeze" }, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::outputGain, 1 }, "Output Gain", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return String (Map::outputDb (v), 1) + " dB"; })));
    return l;
}

//==============================================================================
TexturalAudioProcessor::TexturalAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TEXTURAL", createLayout())
{
    bypassParam = dynamic_cast<AudioParameterBool*> (apvts.getParameter (PID::bypass));

    auto rp = [this] (const String& id) { return apvts.getRawParameterValue (id); };
    raw.time = rp (PID::time);           raw.size = rp (PID::size);             raw.overlap = rp (PID::overlap);
    raw.regen = rp (PID::regen);         raw.pitch = rp (PID::pitch);           raw.mix = rp (PID::mix);
    raw.reverse = rp (PID::reverse);     raw.mode = rp (PID::mode);             raw.bypass = rp (PID::bypass);
    raw.freeze = rp (PID::freeze);       raw.timeRnd = rp (PID::timeRnd);       raw.sizeRnd = rp (PID::sizeRnd);
    raw.overlapRnd = rp (PID::overlapRnd); raw.regenMix = rp (PID::regenMix);   raw.pitchRnd = rp (PID::pitchRnd);
    raw.reverseRnd = rp (PID::reverseRnd); raw.reverbMix = rp (PID::reverbMix); raw.reverbDecay = rp (PID::reverbDecay);
    raw.cutoff = rp (PID::cutoff);       raw.quantize = rp (PID::quantize);     raw.vibRate = rp (PID::vibRate);
    raw.vibDepth = rp (PID::vibDepth);   raw.trails = rp (PID::trails);         raw.outputMode = rp (PID::outputMode);
    raw.outputGain = rp (PID::outputGain);

    jassert ((int) ccMap().size() <= kMaxQueued);
    for (int i = 0; i < kMaxQueued; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; }
    jassert ((int) ccMap().size() == kLearnTap);
    for (int i = 0; i < jmin (kMaxQueued, (int) ccMap().size()); ++i)
    {
        ccIsBool[i] = dynamic_cast<AudioParameterBool*> (apvts.getParameter (*ccMap()[(size_t) i].id)) != nullptr;
        if (auto* ch = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (*ccMap()[(size_t) i].id)))
            ccPositions[i] = ch->choices.size();
        if (*ccMap()[(size_t) i].id == PID::bypass) bypassIndex = i;
    }
    resetMidiMap();

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            if (! isGlobalSetting (rp->paramID))
                presetParamIDs.push_back (rp->paramID);

    for (int i = 0; i < 8; ++i) userPresets[(size_t) i] = factorySnapshot (i);
    loadUserPresetFile();
    liveSnapshot = takeSnapshot();
    currentPreset = kLivePreset;
    selectPreset (1);                       // the pedal opens on preset 1 (Sea Synth)
    startTimerHz (60);
}

//==============================================================================
//  MIDI Learn (v1.2)
//==============================================================================
static const char* const kLearnNames[TexturalAudioProcessor::kNumLearn] =
{
    "Time", "Size", "Overlap", "Regen", "Pitch", "Mix",
    "Time Random", "Size Random", "Overlap Random", "Regen Mix", "Pitch Random", "Reverse Random",
    "Reverb Mix", "Reverb Decay", "Filter", "Quantize", "Vibrato Rate", "Vibrato Depth",
    "Reverse", "Mode", "Bypass (footswitch)", "Freeze", "Output Gain", "Output Mode",
    "Trails", "Footswitch Hold Mode", "Preset Scroll Mute",
    "Tap Tempo", "Next Preset"
};

const String& TexturalAudioProcessor::learnId (int i)
{
    static const String tapId { "tapTempo" }, nextId { "nextPreset" };
    if (i == kLearnTap)  return tapId;
    if (i == kLearnNext) return nextId;
    return *ccMap()[(size_t) jlimit (0, (int) ccMap().size() - 1, i)].id;
}

const char* TexturalAudioProcessor::learnName (int i) { return kLearnNames[jlimit (0, kNumLearn - 1, i)]; }

int TexturalAudioProcessor::learnDefaultCC (int i)
{
    if (i == kLearnTap)  return kCCTap;
    if (i == kLearnNext) return kCCPresetNext;
    return ccMap()[(size_t) jlimit (0, (int) ccMap().size() - 1, i)].cc;
}

int TexturalAudioProcessor::learnIndexOf (const String& id)
{
    if (id.isEmpty()) return -1;
    for (int i = 0; i < kNumLearn; ++i)
        if (learnId (i) == id) return i;
    return -1;
}

int TexturalAudioProcessor::learnIndexForCC (int cc) const
{
    for (int i = 0; i < kNumLearn; ++i)
        if (ccAssign[i].load (std::memory_order_relaxed) == cc) return i;
    return -1;
}

void TexturalAudioProcessor::resetMidiMap()
{
    for (int i = 0; i < kNumLearn; ++i) ccAssign[i].store (learnDefaultCC (i));
}

const String TexturalAudioProcessor::getProgramName (int index)
{
    return presetName (juce::jlimit (0, kLastPreset - 1, index) + 1);
}

String TexturalAudioProcessor::presetName (int index) const
{
    if (isLive (index)) return String (kLivePreset) + " Live";
    const auto& fp = factoryPresets();
    if (index >= kExtraPreset && index <= kLastPreset) return fp[(size_t) index - 1].name;   // DAW-only presets
    if (index <= (int) fp.size()) return String (index) + " " + fp[(size_t) index - 1].name;
    return {};
}

bool TexturalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    return in == AudioChannelSet::stereo() || in == AudioChannelSet::mono();
}

void TexturalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    filter.prepare (sampleRate);
    vibrato.prepare (sampleRate);
    reverb.prepare (sampleRate);
    preReverb.prepare (sampleRate);
    diffuser.prepare (sampleRate);
    wetBuffer.setSize (2, jmax (1, samplesPerBlock));
    recBuffer.setSize (2, jmax (1, samplesPerBlock));
    lpCoef = 1.0f - std::exp (-MathConstants<float>::twoPi * 15000.0f / (float) sampleRate);
    preAmt.reset (sampleRate, 0.1); diffAmt.reset (sampleRate, 0.1);
    for (auto* s : { &wetG, &dryG, &rvMix, &outGain })
        s->reset (sampleRate, 0.03);
    wetG.setCurrentAndTargetValue (Map::wetGain (value (PID::mix)));
    dryG.setCurrentAndTargetValue (Map::dryGain (value (PID::mix)));
    rvMix.setCurrentAndTargetValue (value (PID::reverbMix));
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (Map::outputDb (value (PID::outputGain))));
    engageFade = rv (raw.bypass) > 0.5f ? 0.0f : 1.0f;
}

//==============================================================================
// Anti-gel v2 (Studio Wiga): no host call (setValueNotifyingHost, triggerAsyncUpdate), no string
// lookup and no allocation on the audio thread. MIDI values are queued; the processor timer
// (60 Hz, message thread) sends them to the host and handles presets / tap tempo.
void TexturalAudioProcessor::handleMidi (const MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isProgramChange())
        {
            pendingPreset = jlimit (0, kLastPreset, m.getProgramChangeNumber());   // 0 = Live too
            continue;
        }
        if (! m.isController()) continue;

        const int cc = m.getControllerNumber();
        const int v  = m.getControllerValue();
        lastMidiCC = cc; lastMidiValue = v; lastMidiTime = Time::getMillisecondCounterHiRes();

        // MIDI Learn : le CC reçu est affecté à la commande visée (il n'agit pas sur le son)
        const int lt = learnTarget.load();
        if (lt >= 0 && lt < kNumLearn && cc < 120)
        {
            for (int j = 0; j < kNumLearn; ++j)
                if (j != lt && ccAssign[j].load() == cc) ccAssign[j].store (-1);   // numéro déjà pris : l'autre commande est désassignée
            ccAssign[lt].store (cc);
            lastLearned.store (lt);
            learnTarget.store (-1);
            continue;
        }

        for (int i = 0; i < kNumLearn; ++i)
        {
            if (ccAssign[i].load (std::memory_order_relaxed) != cc) continue;
            if (i == kLearnTap)  { if (v >= 64) tap(); continue; }
            if (i == kLearnNext) { if (v >= 64) pendingPresetStep.fetch_add (1); continue; }
            if (i >= kMaxQueued) continue;
            float norm = (float) v / 127.0f;
            if (i == bypassIndex) norm = v >= 64 ? 0.0f : 1.0f;              // BYPASS (CC82 par défaut) >= 64 = effect ON
            else if (ccIsBool[i]) norm = v >= 64 ? 1.0f : 0.0f;
            else if (ccPositions[i] > 1)                                       // sélecteur : CC réparti sur les positions
                norm = (float) jmin (ccPositions[i] - 1, v * ccPositions[i] / 128) / (float) (ccPositions[i] - 1);
            pendingValue[i].store (norm, std::memory_order_relaxed);
            pendingSet[i].store (true, std::memory_order_release);
        }
    }
}

void TexturalAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    handleMidi (midi);
    midi.clear();

    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (n == 0) return;
    if (wetBuffer.getNumSamples() < n) wetBuffer.setSize (2, n, false, false, true);
    if (recBuffer.getNumSamples() < n) recBuffer.setSize (2, n, false, false, true);

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    if (numIn == 1 && numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numOut > 1 ? 1 : 0);

    // overlap knob moved after a tap -> OVERLAP takes control again
    const float ov = rv (raw.overlap);
    if (tapSpacing.load() > 0.0f && std::abs (ov - overlapAtTap.load()) > 0.003f) tapSpacing = 0.0f;

    if (clearRequested.exchange (false)) { engine.reset(); reverb.reset(); preReverb.reset(); diffuser.reset(); vibrato.reset(); filter.reset(); }

    const bool engaged = rv (raw.bypass) < 0.5f;
    const bool trails  = rv (raw.trails) > 0.5f;
    const auto outMode = (OutMode) roundToInt (rv (raw.outputMode));

    EngineParams ep;
    ep.mode       = (Mode) roundToInt (rv (raw.mode));
    ep.time       = rv (raw.time);
    ep.size       = rv (raw.size);
    ep.overlap    = ov;
    ep.regen      = rv (raw.regen);
    ep.pitch      = rv (raw.pitch);
    ep.reverse    = rv (raw.reverse) > 0.5f;
    ep.freeze     = engaged && rv (raw.freeze) > 0.5f;   // freeze disengages on bypass
    ep.timeRnd    = rv (raw.timeRnd);
    ep.sizeRnd    = rv (raw.sizeRnd);
    ep.overlapRnd = rv (raw.overlapRnd);
    ep.regenMix   = rv (raw.regenMix);
    ep.pitchRnd   = rv (raw.pitchRnd);
    ep.reverseRnd = rv (raw.reverseRnd);
    ep.quantize   = rv (raw.quantize) > 0.5f;
    ep.stereo     = outMode == OutMode::Stereo;
    ep.tapSpacing = tapSpacing.load();
    ep.inputMuted = ! engaged && trails;
    ep.mix        = rv (raw.mix);
    ep.cutoff     = rv (raw.cutoff);
    ep.vibDepth   = rv (raw.vibDepth);

    // bypass without trails: fade out, then mute and clear
    const bool processFx = engaged || trails || engageFade > 0.0f;
    if (! processFx)
    {
        if (! wasCleared) { engine.reset(); reverb.reset(); preReverb.reset(); diffuser.reset(); vibrato.reset(); filter.reset(); wasCleared = true; }
        return;   // dry signal passes untouched
    }
    wasCleared = false;

    // REVERSE REVERB: with REVERSE on, the reverb is also placed before the grains,
    // so reversed grains replay reverb tails backwards (swelling "reverse reverb").
    preAmt.setTargetValue (0.0f);   // reverse plays the recorded guitar itself backwards (no pre-reverb)
    preReverb.setTone (rv (raw.cutoff));
    preReverb.setDecay (jmin (12.0f, Map::reverbRT60 (rv (raw.reverbDecay))));
    {
        float* rl = recBuffer.getWritePointer (0);
        float* rr = recBuffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float a = preAmt.getNextValue();
            if (a > 0.0005f)
            {
                float pl, pr;
                preReverb.process (inL[i], inR[i], pl, pr);
                rl[i] = inL[i] * (1.0f - 0.35f * a) + 1.0f * a * pl;
                rr[i] = inR[i] * (1.0f - 0.35f * a) + 1.0f * a * pr;
            }
            else { rl[i] = inL[i]; rr[i] = inR[i]; }
        }
    }

    engine.process (inL, inR, recBuffer.getReadPointer (0), recBuffer.getReadPointer (1),
                    wetBuffer.getWritePointer (0), wetBuffer.getWritePointer (1), n, ep);

    // the denser the cloud, the more the grains are blurred together (planing texture)
    diffAmt.setTargetValue (jlimit (0.0f, 0.75f, (Map::overlapCount (ov) - 3.0f) / 16.0f));

    const float mix = rv (raw.mix);
    wetG.setTargetValue (Map::wetGain (mix));
    dryG.setTargetValue (engaged ? Map::dryGain (mix) : 1.0f);
    rvMix.setTargetValue (rv (raw.reverbMix));
    outGain.setTargetValue (Decibels::decibelsToGain (Map::outputDb (rv (raw.outputGain))));
    filter.setKnob (rv (raw.cutoff));
    vibrato.set (Map::vibratoHz (rv (raw.vibRate)), Map::vibratoMs (rv (raw.vibDepth)));
    reverb.setTone (rv (raw.cutoff));
    reverb.setDecay (Map::reverbRT60 (rv (raw.reverbDecay)));

    const float fadeStep = 1.0f / (0.015f * (float) sr);
    const float* wl = wetBuffer.getReadPointer (0);
    const float* wr = wetBuffer.getReadPointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float xL = inL[i], xR = inR[i];

        float wL = wl[i], wR = wr[i];
        // DC blocker + gentle 15 kHz top-end smoothing (pitched-up grains stay sweet)
        const float yL = wL - dcXL + 0.9995f * dcL; dcXL = wL; dcL = yL;
        const float yR = wR - dcXR + 0.9995f * dcR; dcXR = wR; dcR = yR;
        lpL += lpCoef * (yL - lpL); lpR += lpCoef * (yR - lpR);
        wL = lpL; wR = lpR;
        diffuser.process (wL, wR, diffAmt.getNextValue());
        vibrato.process (wL, wR);
        if (ep.mode == Mode::Layers)   // warm analog-synth style saturation on the layered voices
        {
            wL = std::tanh (1.8f * wL) * 0.62f;
            wR = std::tanh (1.8f * wR) * 0.62f;
        }
        // wet-bus level control (keeps dense / regenerating textures musical)
        const float wpk = jmax (std::abs (wL), std::abs (wR));
        wetEnv += (wpk > wetEnv ? 0.01f : 0.00008f) * (wpk - wetEnv);
        const float agc = wetEnv > 0.4f ? 0.4f / wetEnv : 1.0f;
        wL *= agc; wR *= agc;
        const float wg = wetG.getNextValue();
        wL *= wg; wR *= wg;

        const float dg = dryG.getNextValue();
        const float dL = xL * dg, dR = xR * dg;

        // both wet and dry go through the reverb (mix happens pre-reverb)
        float rL, rR;
        reverb.process (wL + (engaged ? dL : 0.0f), wR + (engaged ? dR : 0.0f), rL, rR);
        const float rm = rvMix.getNextValue();
        float pL = wL + rL * rm, pR = wR + rR * rm;
        if (outMode != OutMode::Stereo) { const float m = 0.5f * (pL + pR); pL = pR = m; }
        filter.process (pL, pR);

        float oL, oR;
        if (outMode == OutMode::WetDry && engaged) { oL = pL; oR = 0.5f * (dL + dR); }
        else                                       { oL = pL + dL; oR = pR + dR; }

        const float g = outGain.getNextValue();
        oL *= g; oR *= g;
        // gentle safety limiter
        auto lim = [] (float x) { const float a = std::abs (x); return a < 0.9f ? x : std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.0f), x); };
        oL = lim (oL); oR = lim (oR);

        if (! trails)
        {
            engageFade = jlimit (0.0f, 1.0f, engageFade + (engaged ? fadeStep : -fadeStep));
            oL = xL + (oL - xL) * engageFade;
            oR = xR + (oR - xR) * engageFade;
        }
        else engageFade = engaged ? 1.0f : 0.0f;

        outL[i] = oL;
        if (outR != nullptr) outR[i] = oR;
    }

    for (int ch = 2; ch < numOut; ++ch) buffer.clear (ch, 0, n);

    // safety net: never let a non-finite value reach the host
    for (int ch = 0; ch < jmin (2, numOut); ++ch)
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
            {
                buffer.clear();
                engine.reset(); reverb.reset(); preReverb.reset(); diffuser.reset(); vibrato.reset(); filter.reset();
                return;
            }
}

//==============================================================================
// Presets
std::map<String, float> TexturalAudioProcessor::takeSnapshot() const
{
    std::map<String, float> s;
    for (auto& id : presetParamIDs) s[id] = apvts.getParameter (id)->getValue();
    return s;
}

std::map<String, float> TexturalAudioProcessor::factorySnapshot (int slot) const
{
    std::map<String, float> s;
    for (auto& id : presetParamIDs)
        s[id] = apvts.getParameter (id)->getDefaultValue();
    const auto& fp = factoryPresets()[(size_t) slot];
    for (auto& [id, v] : fp.values)
        if (auto* p = apvts.getParameter (id))
            s[id] = p->convertTo0to1 (v);
    return s;
}

void TexturalAudioProcessor::applySnapshot (const std::map<String, float>& s)
{
    for (auto& [id, v] : s)
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (v);
            p->endChangeGesture();
        }
}

void TexturalAudioProcessor::selectPreset (int index)
{
    index = jlimit (0, kLastPreset, index);
    if (index == 0) index = kLivePreset;
    const int prev = currentPreset.load();
    if (isLive (prev) && ! isLive (index)) liveSnapshot = takeSnapshot();      // remember live mode settings

    if (isLive (index)) applySnapshot (liveSnapshot);
    else if (index >= kExtraPreset) applySnapshot (factorySnapshot (index - 1));          // not on the pedal slots
    else            applySnapshot (userPresets[(size_t) index - 1]);

    loadedSnapshot = takeSnapshot();
    currentPreset = index;
    if (value (PID::presetMute) > 0.5f) clearRequested = true;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

// PRESETS button: 1 -> 2 (Live) -> 3 ... 8 -> 1
void TexturalAudioProcessor::nextPreset()
{
    const int c = currentPreset.load();
    const int base = (c >= 1 && c <= 8) ? c : 0;
    selectPreset (base % 8 + 1);
}
void TexturalAudioProcessor::previousPreset()
{
    const int c = currentPreset.load();
    const int base = (c >= 1 && c <= 8) ? c : 1;
    selectPreset (base == 1 ? 8 : base - 1);
}

void TexturalAudioProcessor::savePresetTo (int slot)
{
    if (slot < 1 || slot > 8 || slot == kLivePreset) return;
    userPresets[(size_t) slot - 1] = takeSnapshot();
    loadedSnapshot = takeSnapshot();
    currentPreset = slot;
    saveUserPresetFile();
}

void TexturalAudioProcessor::resetPresetToFactory (int slot)
{
    if (slot < 1 || slot > 8 || slot == kLivePreset) return;
    userPresets[(size_t) slot - 1] = factorySnapshot (slot - 1);
    saveUserPresetFile();
    if (currentPreset.load() == slot) selectPreset (slot);
}

void TexturalAudioProcessor::resetAllPresets()
{
    for (int i = 0; i < 8; ++i) userPresets[(size_t) i] = factorySnapshot (i);
    saveUserPresetFile();
    if (! isLive (currentPreset.load())) selectPreset (currentPreset.load());
}

bool TexturalAudioProcessor::isPresetModified() const
{
    if (isLive (currentPreset.load())) return false;
    for (auto& [id, v] : loadedSnapshot)
        if (std::abs (apvts.getParameter (id)->getValue() - v) > 0.004f) return true;
    return false;
}

String TexturalAudioProcessor::snapshotToString (const std::map<String, float>& s)
{
    String out;
    for (auto& [id, v] : s) out << id << "=" << String (v, 5) << ";";
    return out;
}

std::map<String, float> TexturalAudioProcessor::stringToSnapshot (const String& str)
{
    std::map<String, float> s;
    for (auto& tok : StringArray::fromTokens (str, ";", ""))
        if (tok.contains ("="))
            s[tok.upToFirstOccurrenceOf ("=", false, false)] = tok.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
    return s;
}

File TexturalAudioProcessor::userPresetFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile ("Studio Wiga").getChildFile ("Textural").getChildFile ("UserPresets_v4.xml");
}

void TexturalAudioProcessor::loadUserPresetFile()
{
    auto f = userPresetFile();
    if (! f.existsAsFile()) return;
    if (auto xml = XmlDocument::parse (f))
        for (int i = 0; i < 8; ++i)
            if (auto* e = xml->getChildByName ("P" + String (i + 1)))
            {
                auto s = stringToSnapshot (e->getStringAttribute ("v"));
                for (auto& [id, v] : s) userPresets[(size_t) i][id] = v;
            }
}

void TexturalAudioProcessor::saveUserPresetFile() const
{
    XmlElement root ("TEXTURAL_PRESETS");
    for (int i = 0; i < 8; ++i)
        root.createNewChildElement ("P" + String (i + 1))->setAttribute ("v", snapshotToString (userPresets[(size_t) i]));
    auto f = userPresetFile();
    f.getParentDirectory().createDirectory();
    root.writeTo (f);
}

//==============================================================================
// Tap tempo: hold REVERSE + tap MODE, Ctrl+click MODE, or MIDI CC83
void TexturalAudioProcessor::tap()
{
    const double now = Time::getMillisecondCounterHiRes();
    const double interval = now - lastTapMs.load();
    lastTapMs = now;
    if (interval < 50.0 || interval > 2000.0) { tapCount = 0; return; }

    tapIntervals[tapCount % 3] = interval;
    ++tapCount;
    const int c = jmin (tapCount, 3);
    double avg = 0;
    for (int i = 0; i < c; ++i) avg += tapIntervals[i];
    avg /= c;

    tapBpm = (float) (60000.0 / avg);
    tapSpacing = (float) (avg / 1000.0 / 2.0);          // grains on eighth notes
    overlapAtTap = rv (raw.overlap);                   // tap() may run on the audio thread (MIDI CC83): no string lookup
    pendingTapSeconds = (float) (avg / 1000.0);      // applied by the timer (message thread)
}

void TexturalAudioProcessor::timerCallback()
{
    const auto& map = ccMap();
    for (int i = 0; i < jmin (kMaxQueued, (int) map.size()); ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* p = apvts.getParameter (*map[(size_t) i].id))
                p->setValueNotifyingHost (pendingValue[i].load (std::memory_order_relaxed));

    const int p = pendingPreset.exchange (-1);
    if (p >= 0) selectPreset (p);

    int steps = pendingPresetStep.exchange (0);
    while (steps-- > 0) nextPreset();

    const float t = pendingTapSeconds.exchange (0.0f);
    if (t > 0.0f && currentMode() != Mode::Layers)       // tap syncs delay time (no effect in Layers)
        if (auto* tp = apvts.getParameter (PID::time))
        {
            tp->beginChangeGesture();
            tp->setValueNotifyingHost (Map::timeToNorm (t));
            tp->endChangeGesture();
        }
}

//==============================================================================
String TexturalAudioProcessor::describe (const String& id) const
{
    const float v = value (id);
    const Mode m = currentMode();
    auto secs = [] (float s) { return s < 1.0f ? String (roundToInt (s * 1000.0f)) + " ms" : String (s, 2) + " s"; };
    auto pc   = [] (float x) { return String (roundToInt (x * 100.0f)) + "%"; };
    auto onoff = [] (float x) { return x > 0.5f ? String ("ON") : String ("OFF"); };

    if (id == PID::time)    return (m == Mode::Layers ? "ATTACK " : "TIME ") + secs (Map::timeSeconds (v, m));
    if (id == PID::size)    return "SIZE " + secs (Map::sizeSeconds (v, m));
    if (id == PID::overlap) return "OVERLAP " + String (roundToInt (Map::overlapCount (v))) + (tapSpacing.load() > 0 ? " (TAP)" : "");
    if (id == PID::regen)   return m == Mode::Layers ? (v >= 0.985f ? String ("DECAY INF") : "DECAY " + secs (Map::layerDecaySeconds (v)))
                                                     : "REGEN " + pc (v);
    if (id == PID::pitch)
    {
        float s = Map::pitchSemis (v);
        if (value (PID::quantize) > 0.5f) s = Map::quantiseSemis (s);
        return "PITCH " + String (s > 0 ? "+" : "") + String (s, 1) + " st";
    }
    if (id == PID::mix)        return "MIX " + pc (v) + (v > 0.75f ? "  +PARTICLES " + pc ((v - 0.75f) / 0.25f) : String());
    if (id == PID::timeRnd)    return "TIME RND " + pc (v);
    if (id == PID::sizeRnd)    return "SIZE RND " + pc (v);
    if (id == PID::overlapRnd) return "OVERLAP RND " + pc (v);
    if (id == PID::regenMix)   return "REGEN MIX " + String (roundToInt ((1.0f - v) * 100)) + "/" + String (roundToInt (v * 100)) + " DLY/GRN";
    if (id == PID::pitchRnd)   return v <= 0.33f ? "DETUNE " + String (roundToInt (v / 0.33f * 30.0f)) + " cents"
                                                 : "PITCH RND " + pc ((v - 0.33f) / 0.67f);
    if (id == PID::reverseRnd) return "REVERSE RND " + pc (Map::reverseProbability (v));
    if (id == PID::reverbMix)  return "REVERB MIX " + pc (v);
    if (id == PID::reverbDecay)return v >= 0.97f ? String ("REVERB DECAY INFINITE") : "REVERB DECAY " + secs (Map::reverbRT60 (v));
    if (id == PID::cutoff)
    {
        if (std::abs (v - 0.5f) < 0.015f) return String ("FILTER FLAT");
        if (v < 0.5f) { const float f = 180.0f * std::pow (20000.0f / 180.0f, v / 0.5f); return "LP " + (f >= 1000 ? String (f / 1000, 1) + " kHz" : String (roundToInt (f)) + " Hz"); }
        const float f = 20.0f * std::pow (200.0f, (v - 0.5f) / 0.5f);
        return "HP " + (f >= 1000 ? String (f / 1000, 1) + " kHz" : String (roundToInt (f)) + " Hz");
    }
    if (id == PID::quantize)   return "QUANTIZE " + onoff (v);
    if (id == PID::vibRate)    return "VIB RATE " + String (Map::vibratoHz (v), 2) + " Hz";
    if (id == PID::vibDepth)   return "VIB DEPTH " + String (Map::vibratoMs (v), 2) + " ms";
    if (id == PID::presetMute) return "PRESET MUTE " + onoff (v);
    if (id == PID::trails)     return "TRAILS " + onoff (v);
    if (id == PID::outputMode) { static const char* n[] = { "MONO", "WET/DRY", "STEREO" }; return "OUTPUT " + String (n[jlimit (0, 2, roundToInt (v))]); }
    if (id == PID::fsHoldMode) return v > 0.5f ? String ("FS HOLD FREEZE") : String ("FS HOLD P.SCROLL");
    if (id == PID::outputGain) return "GAIN " + String (Map::outputDb (v), 1) + " dB";
    if (auto* p = apvts.getParameter (id)) return p->getName (20).toUpperCase() + " " + p->getCurrentValueAsText();
    return {};
}

//==============================================================================
void TexturalAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.setProperty ("uiHeight", uiHeight.load(), nullptr);
    for (int i = 0; i < kNumLearn; ++i)
        state.setProperty ("cc_" + learnId (i), ccAssign[i].load(), nullptr);
    state.setProperty ("live", snapshotToString (isLive (currentPreset.load()) ? takeSnapshot() : liveSnapshot), nullptr);
    state.setProperty ("loaded", snapshotToString (loadedSnapshot), nullptr);
    for (int i = 0; i < 8; ++i)
        state.setProperty ("user" + String (i + 1), snapshotToString (userPresets[(size_t) i]), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TexturalAudioProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = ValueTree::fromXml (*xml);

    for (int i = 0; i < 8; ++i)
    {
        const String s = state.getProperty ("user" + String (i + 1)).toString();
        if (s.isNotEmpty())
        {
            auto snap = stringToSnapshot (s);
            for (auto& [id, v] : snap) userPresets[(size_t) i][id] = v;
        }
    }
    liveSnapshot   = stringToSnapshot (state.getProperty ("live").toString());
    loadedSnapshot = stringToSnapshot (state.getProperty ("loaded").toString());
    currentPreset  = jlimit (0, kLastPreset, (int) state.getProperty ("preset", kLivePreset));
    if (currentPreset.load() == 0) currentPreset = kLivePreset;
    uiHeight       = jlimit (493, 1479, (int) state.getProperty ("uiHeight", 493));
    // assignations MIDI Learn (ancien projet sans ces propriétés : CC fixes d'avant)
    for (int i = 0; i < kNumLearn; ++i)
        ccAssign[i].store (jlimit (-1, 127, (int) state.getProperty ("cc_" + learnId (i), learnDefaultCC (i))));
    apvts.replaceState (state);
}

AudioProcessorEditor* TexturalAudioProcessor::createEditor() { return new TexturalAudioProcessorEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TexturalAudioProcessor(); }
