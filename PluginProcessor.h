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

    int getNumPrograms() override                          { return 9; }
    int getCurrentProgram() override                       { return currentPreset.load(); }
    void setCurrentProgram (int index) override            { selectPreset (index); }
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
    FDNReverb      reverb;
    juce::AudioBuffer<float> wetBuffer;
    juce::SmoothedValue<float> wetG, dryG, rvMix, outGain;
    float engageFade = 1.0f, wetEnv = 0.0f;
    bool  wasCleared = false;
    double sr = 44100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TexturalAudioProcessor)
};
