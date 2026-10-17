#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
//  Textural editor: the Studio Wiga faceplate with vector knobs, buttons and
//  LEDs drawn in code. Everything is laid out in "design units" (595 x 1000)
//  and scaled to the window (default height 493 px).
//
//  Secondary layers (like holding the buttons on the pedal):
//    Ctrl  + drag a knob  -> REVERSE layer   (or long-press REVERSE to latch)
//    Alt   + drag a knob  -> MODE layer      (or long-press MODE to latch)
//    Ctrl+Alt + drag      -> FOOTSWITCH layer (or click the blue LED to latch)
//==============================================================================
class TexturalAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit TexturalAudioProcessorEditor (TexturalAudioProcessor&);
    ~TexturalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr float kDesignW = 595.0f;
    static constexpr float kDesignH = 1000.0f;
    static constexpr int   kDefaultH = 493;

private:
    enum class Layer { Main, Reverse, Mode, Foot };
    enum class Hit { None, Knob, ReverseBtn, ModeBtn, Footswitch, Led, PresetBtn };

    void timerCallback() override;
    float scale() const { return (float) getHeight() / kDesignH; }
    juce::Point<float> toDesign (const juce::MouseEvent& e) const { return e.position / scale(); }
    Hit  hitTest (juce::Point<float> p, int& knobIndex) const;
    Layer currentLayer() const;
    juce::String knobParamID (Layer, int knob) const;
    void showMenu();
    void setHeightKeepingAspect (int h);
    void setParam (const juce::String& id, float normalised);
    void touch (const juce::String& id);
    void showKnob (int knob, const juce::String& id);

    // drawing
    void drawKnob (juce::Graphics&, juce::Point<float> c, float r, float angle, bool orange) const;
    void drawLedLens (juce::Graphics&, juce::Point<float> c, float r, juce::Colour col, float on, bool diffused) const;
    void drawRoundButton (juce::Graphics&, juce::Point<float> c, float r, juce::Colour lit, float amount) const;
    void drawStatusLed (juce::Graphics&, juce::Point<float> c, float on) const;
    void drawFootswitch (juce::Graphics&, juce::Point<float> c, bool pressed) const;
    void drawPresetLed (juce::Graphics&, juce::Point<float> c, juce::Colour col, float on) const;
    void drawPresetButton (juce::Graphics&, juce::Point<float> c, bool pressed) const;
    void drawStatusLine (juce::Graphics&) const;

    TexturalAudioProcessor& proc;
    juce::Image faceplate, footswitchImg;
    static constexpr int   kKnobFrames  = 30;     // cap frames over one 360/7 degree lobe period
    static constexpr int   kKnobFramePx = 276;    // 2x resolution sprites
    juce::Image knobFrames[2][kKnobFrames], knobBase[2], knobPointer[2];

    // interaction state
    Layer latched = Layer::Main;
    Layer dragLayer = Layer::Main;
    Hit   pressedHit = Hit::None;
    int   dragKnob = -1;
    float dragStartValue = 0, dragStartY = 0;
    juce::String dragParam;
    double pressStartMs = 0;
    bool  longPressHandled = false;
    bool  fsFreezeHeld = false;

    bool  presetScrollMode = false;
    double presetScrollUntil = 0;
    bool  copyMode = false;
    int   copyTarget = 1;
    double copyUntil = 0;

    int statusKnob = -1;
    juce::String statusParam;
    double statusMs = 0, lastMidiSeen = 0;
    juce::String lastTouched;
    double lastTouchedMs = 0;
    double phase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TexturalAudioProcessorEditor)
};
