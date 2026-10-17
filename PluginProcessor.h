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
                               private juce::AsyncUpdater
{
public:
    TexturalAudioProcessor();
    ~TexturalAudioProcessor() override = default;

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
    static constexpr int kLastPreset  = 10;                // last one (Aurora Grains)
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

    // UI
    std::atomic<int> uiHeight { 493 };
    bool isFrozen() const       { return engine.frozen.load(); }
    int  activeLayers() const   { return engine.layersActive.load(); }

private:
    void handleAsyncUpdate() override;
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
