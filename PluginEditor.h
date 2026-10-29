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
    void parentHierarchyChanged() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Window: same size and proportions as Freeze Shimmer (Studio Wiga), 780 x 990 artwork.
    // Default 388 x 493 px, menu sizes 60 % / 75 % / 100 % = 592 / 740 / 986 px, drag up to 1479 px.
    static constexpr float kDesignH = 1000.0f;
    static constexpr float kDesignW = 780.0f * kDesignH / 990.0f;
    static constexpr int   kDefaultH = 493, kH60 = 592, kH75 = 740, kH100 = 986, kMaxH = 1479;
    // Pedal space: the layout the control coordinates are given in (photo 1246 x 1805 scaled
    // to 1000 units high, with a 202.8-unit margin on each side). It is mapped into the window
    // by pedalTransform().
    static constexpr float kSideMargin = 100.0f * 1000.0f / 493.0f;
    static constexpr float kPedalSpaceW = 1246.0f * 1000.0f / 1805.0f + 2.0f * kSideMargin;

private:
    enum class Layer { Main, Reverse, Mode, Foot };
    enum class Hit { None, Knob, ReverseBtn, ModeBtn, Footswitch, Led, PresetBtn };

    void timerCallback() override;
    void visibilityChanged() override;
    void useSoftwareRenderer();
    // anti-gel v2: background + faceplate composed at window size (physical pixels), rebuilt by the timer only
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    void askRepaint (juce::Rectangle<int> r);
    juce::Image bgCache;
    int cacheW = 0, cacheH = 0;
    float cacheScale = 0.0f;
    int resizeSettle = 0;
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;
    float scale() const { return (float) getHeight() / kDesignH; }
    // Pedal size and position copied from Freeze Shimmer: the enclosure is 601 x 928 in its
    // 780 x 990 window (about 299 x 462 px at 493 px), starting 29/990 from the top.
    // The photo's enclosure occupies rows 7..1797 and columns 44..1203 of 1246 x 1805 px,
    // the same 601:928 proportions.
    static constexpr float kBodyFrac   = 1790.0f / 1805.0f;
    static constexpr float kPedalScale = (928.0f / 990.0f) / kBodyFrac;
    static constexpr float kPedalTop   = 29.0f / 990.0f * kDesignH - kPedalScale * 7.0f / 1805.0f * kDesignH;
    static juce::AffineTransform pedalTransform()
    {
        return juce::AffineTransform::translation (-kPedalSpaceW * 0.5f, 0.0f)
                   .scaled (kPedalScale).translated (kDesignW * 0.5f, kPedalTop);
    }
    juce::Point<float> toDesign (const juce::MouseEvent& e) const
    {
        return (e.position / scale()).transformedBy (pedalTransform().inverted());
    }
    Hit  hitTest (juce::Point<float> p, int& knobIndex) const;
    Layer currentLayer() const;
    juce::String knobParamID (Layer, int knob) const;
    void showMenu (int learnIdx = -1);     // learnIdx >= 0 : clic droit sur une commande (MIDI Learn en tête du menu)
    int  learnIndexAt (const juce::MouseEvent&) const;
    void setHeightKeepingAspect (int h);
    void setParam (const juce::String& id, float normalised);
    void touch (const juce::String& id);
    void showKnob (int knob, const juce::String& id);
    void showMessage (const juce::String& text, juce::Colour colour);

    // drawing
    void drawKnob (juce::Graphics&, juce::Point<float> c, float r, float angle, bool orange) const;
    void drawLedLens (juce::Graphics&, juce::Point<float> c, float r, juce::Colour col, float on, bool diffused) const;
    void drawRoundButton (juce::Graphics&, juce::Point<float> c, float r, juce::Colour lit, float amount) const;
    void drawStatusLed (juce::Graphics&, juce::Point<float> c, float on) const;
    void drawFootswitch (juce::Graphics&, juce::Point<float> c, bool pressed) const;
    void drawPresetLed (juce::Graphics&, juce::Point<float> c, juce::Colour col, float on) const;
    void drawPresetButton (juce::Graphics&, juce::Point<float> c, bool pressed) const;
    void drawStatusLine (juce::Graphics&) const;
    bool statusLineState (juce::String& txt, juce::Colour& col, float& alpha) const;

    // état affiché des éléments animés (partagé par paint() et timerCallback())
    bool  blink (double hz) const { return std::fmod (phase * hz, 1.0) < 0.5; }
    float knobValue (Layer, int knob) const;
    float reverseAmount (Layer) const;
    void  modeLedState (Layer, juce::Colour& c, float& amt) const;
    float statusLedAmount (Layer) const;
    void  presetLedState (int i, int cur, bool modified, juce::Colour& c, float& on) const;
    juce::Rectangle<int> pedalZone (juce::Point<float> c, float r) const;

    TexturalAudioProcessor& proc;
    juce::Image faceplate, footswitchImg, backgroundImg;
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
    juce::String statusParam, statusMsg;
    juce::Colour statusMsgColour;
    double statusMs = 0, lastMidiSeen = 0;
    bool learnMsgShown = false;          // message « MIDI LEARN: ... » affiché
    juce::String lastTouched;
    double lastTouchedMs = 0;
    double phase = 0;

    // dernier état réellement dessiné : on ne redessine une zone que si elle a changé
    float drawnKnob[6] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float drawnReverse = -1.0f, drawnMode = -1.0f, drawnLed = -1.0f, drawnStatusAlpha = -1.0f;
    float drawnPresetLed[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    juce::Colour drawnModeColour, drawnPresetColour[4], drawnStatusColour;
    juce::String drawnStatus;
    Hit drawnPressed = Hit::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TexturalAudioProcessorEditor)
};
