#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "Presets.h"
#include "GranularEngine.h"
#include "Effects.h"
#include <map>

//==============================================================================
//  Textural (Studio Wiga) - granular synth engine pedal, VST3
//==============================================================================
class TexturalAudioProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    TexturalAudioProcessor();
    ~TexturalAudioProcessor() override { stopTimer(); }

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return "Textural"; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 20.0; }

    static constexpr int kExtraPreset = 9;                 // first DAW / menu-only preset (Slow Attack Soundtrack)
    static constexpr int kLastPreset  = 9;                 // last one
    static constexpr int kLivePreset  = 2;                 // pedal position 2 = Live mode
    static bool isLive (int i) { return i == 0 || i == kLivePreset; }
    // DAW program list: 0..7 = pedal positions 1..8 (Live appears once, as "2 Live"), 8 = extra preset
    int getNumPrograms() override                          { return kLastPreset; }
    int getCurrentProgram() override                       { const int c = currentPreset.load(); return c == 0 ? kLivePreset - 1 : c - 1; }
    void setCurrentProgram (int index) override            { selectPreset (juce::jlimit (0, kLastPreset - 1, index) + 1); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::RangedAudioParameter* param (const juce::String& id) const { return apvts.getParameter (id); }
    float value (const juce::String& id) const                       { return apvts.getRawParameterValue (id)->load(); }
    Mode  currentMode() const                                        { return (Mode) juce::roundToInt (value (PID::mode)); }

    // presets (0 = live mode, 1..8 = presets)
    void selectPreset (int index);
    void nextPreset();
    void previousPreset();
    void savePresetTo (int slot);
    void resetPresetToFactory (int slot);
    void resetAllPresets();
    bool isPresetModified() const;
    juce::String presetName (int index) const;
    std::atomic<int> currentPreset { 0 };

    // tap tempo (thread-safe)
    void tap();
    std::atomic<float> tapBpm { 0.0f };

    // status-line helpers
    juce::String describe (const juce::String& id) const;
    std::atomic<int>    lastMidiCC { -1 }, lastMidiValue { 0 };
    std::atomic<double> lastMidiTime { 0.0 };

    // MIDI Learn (v1.2) : toutes les commandes (CC fixes d'avant = numéros par défaut, réassignables)
    // index 0..26 = entrées de ccMap() (même ordre), 27 = tap tempo, 28 = preset suivant
    static constexpr int kNumLearn = 29, kLearnTap = 27, kLearnNext = 28;
    static const juce::String& learnId (int i);
    static const char* learnName (int i);
    static int learnDefaultCC (int i);
    static int learnIndexOf (const juce::String& id);
    int learnIndexForCC (int cc) const;
    std::atomic<int> ccAssign[kNumLearn];             // numéro de CC de chaque commande, -1 = non assigné
    std::atomic<int> learnTarget { -1 };              // commande en attente d'un CC (-1 = pas de learn)
    std::atomic<int> lastLearned { -1 };              // dernière commande apprise (lue et remise à -1 par la fenêtre)
    void resetMidiMap();

    // UI
    std::atomic<int> uiHeight { 493 };
    bool isFrozen() const       { return engine.frozen.load(); }
    int  activeLayers() const   { return engine.layersActive.load(); }

private:
    void timerCallback() override;
    void handleMidi (const juce::MidiBuffer&);
    void applySnapshot (const std::map<juce::String, float>&);
    std::map<juce::String, float> takeSnapshot() const;
    std::map<juce::String, float> factorySnapshot (int slot) const;
    juce::File userPresetFile() const;
    void loadUserPresetFile();
    void saveUserPresetFile() const;
    static juce::String snapshotToString (const std::map<juce::String, float>&);
    static std::map<juce::String, float> stringToSnapshot (const juce::String&);

    juce::AudioParameterBool* bypassParam = nullptr;
    std::vector<juce::String> presetParamIDs;

    std::array<std::map<juce::String, float>, 8> userPresets;
    std::map<juce::String, float> liveSnapshot, loadedSnapshot;

    // Anti-gel v2 (Studio Wiga): MIDI CC values queued by the audio thread (index = ccMap() entry),
    // sent to the host by the processor timer (60 Hz, message thread)
    static constexpr int kMaxQueued = 32;
    std::atomic<float> pendingValue[kMaxQueued];
    std::atomic<bool>  pendingSet[kMaxQueued];
    bool ccIsBool[kMaxQueued] {};
    int  ccPositions[kMaxQueued] {};                  // paramètre à choix : nombre de positions (CC réparti dessus)
    int  bypassIndex = -1;

    // raw parameter values read by the audio thread (looked up once in the constructor)
    struct Raw
    {
        std::atomic<float> *time = nullptr, *size = nullptr, *overlap = nullptr, *regen = nullptr, *pitch = nullptr, *mix = nullptr,
                           *reverse = nullptr, *mode = nullptr, *bypass = nullptr, *freeze = nullptr,
                           *timeRnd = nullptr, *sizeRnd = nullptr, *overlapRnd = nullptr, *regenMix = nullptr, *pitchRnd = nullptr,
                           *reverseRnd = nullptr, *reverbMix = nullptr, *reverbDecay = nullptr, *cutoff = nullptr, *quantize = nullptr,
                           *vibRate = nullptr, *vibDepth = nullptr, *trails = nullptr, *outputMode = nullptr, *outputGain = nullptr;
    } raw;
    static float rv (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed); }

    // async requests from the audio thread
    std::atomic<int>   pendingPreset { -1 };
    std::atomic<int>   pendingPresetStep { 0 };
    std::atomic<float> pendingTapSeconds { 0.0f };
    std::atomic<bool>  clearRequested { false };

    // tap tempo
    std::atomic<double> lastTapMs { 0.0 };
    std::atomic<float>  tapSpacing { 0.0f };
    std::atomic<float>  overlapAtTap { -1.0f };
    double tapIntervals[3] {};
    int    tapCount = 0;

    // DSP
    GranularEngine engine;
    DualFilter     filter;
    Vibrato        vibrato;
    FDNReverb      reverb;          // main plate (after the grains)
    FDNReverb      preReverb;       // reverse reverb: plate before the reversed grains
    Diffuser       diffuser;
    juce::AudioBuffer<float> wetBuffer, recBuffer;
    float dcL = 0, dcR = 0, dcXL = 0, dcXR = 0, lpL = 0, lpR = 0, lpCoef = 0.8f;
    juce::SmoothedValue<float> preAmt, diffAmt;
    juce::SmoothedValue<float> wetG, dryG, rvMix, outGain;
    float engageFade = 1.0f, wetEnv = 0.0f;
    bool  wasCleared = false;
    double sr = 44100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TexturalAudioProcessor)
};
