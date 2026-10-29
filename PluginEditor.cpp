#include "PluginEditor.h"

using namespace juce;

namespace
{
    struct KnobPos { float x, y, r; bool orange; };
    // positions in design units (faceplate 595 x 1000)
    // Layout in design units: the pedal photo (1246 x 1805 px, widened between the knob columns
    // to match Freeze Shimmer) is scaled to 1000 units high
    // and sits between two side margins of 100 px each (pedal space, see pedalTransform()).
    const KnobPos kKnobs[6] = {
        { 321.7f, 162.6f, 64.0f, false },   // TIME
        { 547.7f, 162.6f, 64.0f, false },   // SIZE
        { 771.6f, 161.5f, 64.0f, false },   // OVERLAP
        { 320.0f, 333.2f, 64.0f, false },   // REGEN
        { 543.9f, 330.2f, 64.0f, true  },   // PITCH
        { 771.6f, 334.9f, 64.0f, false },   // MIX
    };
    const Point<float> kReverseBtn { 460.2f, 69.0f };
    const Point<float> kModeBtn    { 684.0f, 69.0f };
    constexpr float    kBtnR       = 19.0f;
    const Point<float> kLed        { 544.7f, 583.7f };
    const Point<float> kFootswitch { 544.7f, 895.3f };   // footswitch at the bottom
    constexpr float    kFootswitchR = 73.1f;
    const float        kPresetLedX[4] = { 454.1f, 500.7f, 547.2f, 592.6f };
    constexpr float    kPresetLedY = 754.2f, kPresetLedR = 14.6f;
    const Point<float> kPresetBtn  { 640.3f, 753.1f };
    constexpr float    kPedalX = TexturalAudioProcessorEditor::kSideMargin;   // faceplate drawn from here

    // flashy, high-contrast LED colours (mode LED keeps the hue of its symbol)
    const Colour kModeColours[3] = { Colour (0xff39ff2e),    // Delay  (green tree)
                                     Colour (0xffff7b00),    // Auto Freeze (orange asterisk)
                                     Colour (0xffff1238) };  // Layers (red rings)
    const Colour kReverseColour (0xff00d5ff);
    const Colour kPresetGreen   (0xff00ff66);
    const Colour kPresetRed     (0xffff0040);

    double nowMs() { return Time::getMillisecondCounterHiRes(); }
}

//==============================================================================
TexturalAudioProcessorEditor::TexturalAudioProcessorEditor (TexturalAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    faceplate = ImageCache::getFromMemory (BinaryData::faceplate_png, BinaryData::faceplate_pngSize);
    footswitchImg = ImageCache::getFromMemory (BinaryData::footswitch_png, BinaryData::footswitch_pngSize);
    backgroundImg = ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);

    // knob photos: static base, rotating 7-lobe cap (light kept fixed), pointer sprite
    const Image caps[2] = { ImageCache::getFromMemory (BinaryData::knob_cap_black_png,  BinaryData::knob_cap_black_pngSize),
                            ImageCache::getFromMemory (BinaryData::knob_cap_orange_png, BinaryData::knob_cap_orange_pngSize) };
    knobBase[0]    = ImageCache::getFromMemory (BinaryData::knob_base_black_png,  BinaryData::knob_base_black_pngSize);
    knobBase[1]    = ImageCache::getFromMemory (BinaryData::knob_base_orange_png, BinaryData::knob_base_orange_pngSize);
    knobPointer[0] = ImageCache::getFromMemory (BinaryData::knob_ptr_black_png,   BinaryData::knob_ptr_black_pngSize);
    knobPointer[1] = ImageCache::getFromMemory (BinaryData::knob_ptr_orange_png,  BinaryData::knob_ptr_orange_pngSize);
    for (int k = 0; k < 2; ++k)
        for (int f = 0; f < kKnobFrames; ++f)
            knobFrames[k][f] = caps[k].getClippedImage ({ 0, f * kKnobFramePx, kKnobFramePx, kKnobFramePx }).createCopy();

    setResizable (true, true);
    setResizeLimits (roundToInt (kDefaultH * kDesignW / kDesignH), kDefaultH,
                     roundToInt (kMaxH * kDesignW / kDesignH), kMaxH);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (kDesignW / kDesignH);
    setOpaque (true);                      // anti-gel : l'hôte ne redessine jamais ce qui est derrière la fenêtre

    setHeightKeepingAspect (jlimit (kDefaultH, kMaxH, proc.uiHeight.load()));
    rebuildCaches();
    startTimerHz (30);
}

TexturalAudioProcessorEditor::~TexturalAudioProcessorEditor() { stopTimer(); }

//==============================================================================
// Anti-gel de la fenêtre (Windows) : JUCE 8 dessine par défaut avec Direct2D (carte graphique).
// Avec certains pilotes (Intel notamment) la fenêtre du plugin cesse de se rafraîchir alors que
// le DAW continue de tourner. Le moteur logiciel est stable partout et largement assez rapide ici.
//
// Anti-gel v2 (Studio Wiga) :
//  1. moteur de rendu logiciel forcé sur Windows, appliqué à chaque nouvelle fenêtre hôte (peer)
//     et revérifié toutes les secondes, jamais en boucle ;
//  2. aucun calcul lourd dans paint() : le fond + la façade sont composés à la taille de la fenêtre
//     par le timer seulement, après stabilisation de la taille ou quand l'échelle d'affichage change
//     (au plus toutes les 250 ms) ;
//  3. chien de garde : si un redessin demandé n'est pas arrivé au bout de 1,5 s alors que la fenêtre
//     est visible, le moteur de rendu est réappliqué et toute la fenêtre est redessinée ;
//  4. côté audio : aucun appel à l'hôte depuis le thread audio (voir PluginProcessor).
void TexturalAudioProcessorEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw)
            peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void TexturalAudioProcessorEditor::parentHierarchyChanged() { useSoftwareRenderer(); }
void TexturalAudioProcessorEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void TexturalAudioProcessorEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

float TexturalAudioProcessorEditor::physicalScale() const
{
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

// fond + façade composés une fois par taille / échelle (constructeur et timer, jamais dans paint)
void TexturalAudioProcessorEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f) sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgCache.isValid()) return;

    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    Image img (Image::RGB, jmax (1, pw), jmax (1, ph), true);
    {
        Graphics g (img);
        g.fillAll (Colours::black);
        g.addTransform (AffineTransform::scale ((float) pw / (float) w, (float) ph / (float) h));
        g.addTransform (AffineTransform::scale (scale()));
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (backgroundImg, Rectangle<float> (0, 0, kDesignW, kDesignH), RectanglePlacement::fillDestination);
        g.addTransform (pedalTransform());
        g.drawImage (faceplate, Rectangle<float> (kPedalX, 0, kPedalSpaceW - 2.0f * kPedalX, kDesignH));
    }
    bgCache = img;
    repaint();
}

void TexturalAudioProcessorEditor::setHeightKeepingAspect (int h)
{
    h = jlimit (kDefaultH, kMaxH, h);
    setSize (roundToInt ((float) h * kDesignW / kDesignH), h);
}

void TexturalAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici (mise à l'échelle Windows 125 / 150 % : l'hôte arrondit les tailles) ;
    // les limites du constrainer (jamais plus petit que la taille par défaut) suffisent
    proc.uiHeight = jlimit (kDefaultH, kMaxH, getHeight());
    resizeSettle = 4;              // le fond sera recomposé par le timer quand la taille ne bouge plus
}

//==============================================================================
TexturalAudioProcessorEditor::Layer TexturalAudioProcessorEditor::currentLayer() const
{
    if (dragKnob >= 0) return dragLayer;
    if (latched != Layer::Main) return latched;
    const auto m = ModifierKeys::getCurrentModifiersRealtime();
    const bool ctrl = m.isCtrlDown() || m.isCommandDown();
    if (ctrl && m.isAltDown()) return Layer::Foot;
    if (ctrl)                  return Layer::Reverse;
    if (m.isAltDown())         return Layer::Mode;
    return Layer::Main;
}

String TexturalAudioProcessorEditor::knobParamID (Layer l, int k) const
{
    static const String* main[6] = { &PID::time,      &PID::size,    &PID::overlap,    &PID::regen,       &PID::pitch,      &PID::mix };
    static const String* rev[6]  = { &PID::timeRnd,   &PID::sizeRnd, &PID::overlapRnd, &PID::regenMix,    &PID::pitchRnd,   &PID::reverseRnd };
    static const String* mode[6] = { &PID::reverbMix, &PID::cutoff,  &PID::vibRate,    &PID::reverbDecay, &PID::quantize,   &PID::vibDepth };
    static const String* foot[6] = { &PID::presetMute, nullptr,      &PID::trails,     &PID::outputMode,  &PID::fsHoldMode, &PID::outputGain };
    const String* const* t = l == Layer::Reverse ? rev : l == Layer::Mode ? mode : l == Layer::Foot ? foot : main;
    return t[k] != nullptr ? *t[k] : String();
}

TexturalAudioProcessorEditor::Hit TexturalAudioProcessorEditor::hitTest (Point<float> p, int& knobIndex) const
{
    knobIndex = -1;
    for (int i = 0; i < 6; ++i)
        if (p.getDistanceFrom ({ kKnobs[i].x, kKnobs[i].y }) < kKnobs[i].r) { knobIndex = i; return Hit::Knob; }
    if (p.getDistanceFrom (kReverseBtn) < kBtnR + 9)  return Hit::ReverseBtn;
    if (p.getDistanceFrom (kModeBtn)    < kBtnR + 9)  return Hit::ModeBtn;
    if (p.getDistanceFrom (kFootswitch) < 72.0f)      return Hit::Footswitch;
    if (p.getDistanceFrom (kLed)        < 34.0f)      return Hit::Led;
    if (p.getDistanceFrom (kPresetBtn)  < 20.0f)      return Hit::PresetBtn;
    return Hit::None;
}

void TexturalAudioProcessorEditor::setParam (const String& id, float v)
{
    if (auto* p = proc.param (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (jlimit (0.0f, 1.0f, v));
        p->endChangeGesture();
    }
    touch (id);
}

void TexturalAudioProcessorEditor::showMessage (const String& text, Colour colour)
{
    statusMsg = text;
    statusMsgColour = colour;
    statusKnob = -1;
    statusMs = nowMs();
}

void TexturalAudioProcessorEditor::showKnob (int knob, const String& id)
{
    statusMsg.clear();
    statusKnob = knob;
    statusParam = id;
    statusMs = nowMs();
}

void TexturalAudioProcessorEditor::touch (const String& id)
{
    lastTouched = id;
    lastTouchedMs = nowMs();
    if (copyMode && id.isNotEmpty() && id != PID::bypass) copyMode = false;   // altering cancels copy
}

// commande sous le clic droit : le potard dans la couche active (Ctrl / Alt / verrou), les boutons, le footswitch
int TexturalAudioProcessorEditor::learnIndexAt (const MouseEvent& e) const
{
    int k;
    switch (hitTest (toDesign (e), k))
    {
        case Hit::Knob:       return TexturalAudioProcessor::learnIndexOf (knobParamID (currentLayer(), k));
        case Hit::ReverseBtn: return TexturalAudioProcessor::learnIndexOf (PID::reverse);
        case Hit::ModeBtn:    return TexturalAudioProcessor::learnIndexOf (PID::mode);
        case Hit::Footswitch: return TexturalAudioProcessor::learnIndexOf (PID::bypass);
        case Hit::PresetBtn:  return TexturalAudioProcessor::kLearnNext;
        case Hit::Led:
        case Hit::None:
        default:              return -1;
    }
}

//==============================================================================
void TexturalAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMenu (learnIndexAt (e)); return; }

    int k;
    const auto p = toDesign (e);
    pressedHit = hitTest (p, k);
    pressStartMs = nowMs();
    longPressHandled = false;

    if (pressedHit == Hit::Knob)
    {
        dragLayer = currentLayer();
        dragParam = knobParamID (dragLayer, k);
        if (dragParam.isEmpty()) { pressedHit = Hit::None; return; }
        dragKnob = k;
        auto* prm = proc.param (dragParam);
        dragStartValue = prm->getValue();
        dragStartY = p.y;
        prm->beginChangeGesture();
        touch (dragParam);
    }
    repaint();
}

void TexturalAudioProcessorEditor::mouseDrag (const MouseEvent& e)
{
    if (dragKnob < 0) return;
    const auto p = toDesign (e);
    const float sens = e.mods.isShiftDown() ? 1000.0f : 260.0f;
    const float dy = (dragStartY - p.y) + (p.x - toDesign (e.withNewPosition (e.mouseDownPosition)).x) * 0.5f;
    if (auto* prm = proc.param (dragParam))
    {
        prm->setValueNotifyingHost (jlimit (0.0f, 1.0f, dragStartValue + dy / sens));
        touch (dragParam);
        showKnob (dragKnob, dragParam);
    }
    repaint();
}

void TexturalAudioProcessorEditor::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const double held = nowMs() - pressStartMs;

    if (dragKnob >= 0)
    {
        if (auto* prm = proc.param (dragParam)) prm->endChangeGesture();
        dragKnob = -1;
    }
    else if (! longPressHandled)
    {
        switch (pressedHit)
        {
            case Hit::ReverseBtn:
                if (latched == Layer::Reverse) latched = Layer::Main;
                else
                {
                    const bool on = proc.value (PID::reverse) < 0.5f;
                    setParam (PID::reverse, on ? 1.0f : 0.0f);
                    showMessage (on ? "REVERSE ON" : "REVERSE OFF", Colour (0xff0a9fd0));
                }
                break;

            case Hit::ModeBtn:
                if (currentLayer() == Layer::Reverse)                 // hold REVERSE + tap MODE = tap tempo
                {
                    proc.tap();
                    touch (PID::overlap);
                    showMessage ("TAP " + String (roundToInt (proc.tapBpm.load())) + " BPM", Colour (0xff121212));
                }
                else if (latched == Layer::Mode) latched = Layer::Main;
                else
                {
                    const int m = (roundToInt (proc.value (PID::mode)) + 1) % 3;
                    setParam (PID::mode, proc.param (PID::mode)->convertTo0to1 ((float) m));
                    static const char* names[3] = { "DELAY MODE", "AUTO FREEZE MODE", "LAYERS MODE" };
                    static const Colour cols[3] = { Colour (0xff1e9e2a), Colour (0xfff26d0f), Colour (0xffd0102c) };
                    showMessage (names[m], cols[m]);
                }
                break;

            case Hit::Footswitch:
                if (presetScrollMode)
                {
                    proc.nextPreset();
                    presetScrollUntil = nowMs() + 6000.0;
                }
                else
                    setParam (PID::bypass, proc.value (PID::bypass) > 0.5f ? 0.0f : 1.0f);
                break;

            case Hit::Led:
                latched = latched == Layer::Foot ? Layer::Main : Layer::Foot;
                break;

            case Hit::PresetBtn:
                if (copyMode)
                {
                    do copyTarget = copyTarget % 8 + 1;                // navigate target slot (1..8, not Live)
                    while (copyTarget == TexturalAudioProcessor::kLivePreset);
                    copyUntil = nowMs() + 12000.0;
                }
                else
                {
                    proc.nextPreset();
                    const int cur = proc.currentPreset.load();
                    showMessage (proc.presetName (cur).toUpperCase(),
                                 cur <= 4 ? Colour (0xff11a83c) : Colour (0xffd0102c));
                }
                break;

            case Hit::None:
            case Hit::Knob:
            default: break;
        }
    }

    if (fsFreezeHeld)
    {
        fsFreezeHeld = false;
        setParam (PID::freeze, 0.0f);
    }
    ignoreUnused (held);
    pressedHit = Hit::None;
    repaint();
}

void TexturalAudioProcessorEditor::mouseMove (const MouseEvent& e)
{
    int k;
    const auto h = hitTest (toDesign (e), k);
    setMouseCursor (h == Hit::None ? MouseCursor::NormalCursor
                                   : h == Hit::Knob ? MouseCursor::UpDownResizeCursor : MouseCursor::PointingHandCursor);
}

void TexturalAudioProcessorEditor::mouseDoubleClick (const MouseEvent& e)
{
    int k;
    if (hitTest (toDesign (e), k) == Hit::Knob)
    {
        const auto id = knobParamID (currentLayer(), k);
        if (auto* prm = proc.param (id)) { setParam (id, prm->getDefaultValue()); showKnob (k, id); }
    }
}

void TexturalAudioProcessorEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    int k;
    if (hitTest (toDesign (e), k) != Hit::Knob) return;
    const auto id = knobParamID (currentLayer(), k);
    if (auto* prm = proc.param (id))
    {
        setParam (id, prm->getValue() + (w.deltaY + w.deltaX) * (e.mods.isShiftDown() ? 0.02f : 0.08f));
        showKnob (k, id);
    }
}

//==============================================================================
void TexturalAudioProcessorEditor::timerCallback()
{
    phase = nowMs() / 1000.0;

    // a knob turned by MIDI CC is shown in the status line as well
    const double mt = proc.lastMidiTime.load();
    if (mt > lastMidiSeen)
    {
        lastMidiSeen = mt;
        const int li = proc.learnIndexForCC (proc.lastMidiCC.load());
        if (li >= 0 && li < TexturalAudioProcessor::kLearnTap && statusMsg.isEmpty())
        {
            const auto& id = TexturalAudioProcessor::learnId (li);
            for (int l = 0; l < 4; ++l)
                for (int k = 0; k < 6; ++k)
                    if (knobParamID ((Layer) l, k) == id) { statusKnob = k; statusParam = id; statusMs = mt; }
        }
    }

    // MIDI Learn : attente puis confirmation 2 s dans la ligne d'état
    {
        const int learned = proc.lastLearned.exchange (-1);
        const int lt = proc.learnTarget.load();
        if (learned >= 0)
        {
            showMessage (String (TexturalAudioProcessor::learnName (learned)) + " = CC " + String (proc.ccAssign[learned].load()),
                         Colour (0xff121212));
            statusMs += 900.0;                 // message plein 2 s (puis le fondu habituel)
            learnMsgShown = false;
        }
        else if (lt >= 0)
        {
            showMessage ("MIDI LEARN: move a control for " + String (TexturalAudioProcessor::learnName (lt)), Colour (0xff121212));
            learnMsgShown = true;
        }
        else if (learnMsgShown)               // learn annulé : le message s'efface
        {
            learnMsgShown = false;
            statusMs = nowMs() - 1100.0;
        }
    }
    const double held = nowMs() - pressStartMs;

    // long presses
    if (pressedHit != Hit::None && pressedHit != Hit::Knob && ! longPressHandled)
    {
        if ((pressedHit == Hit::ReverseBtn || pressedHit == Hit::ModeBtn) && held > 550.0)
        {
            const Layer l = pressedHit == Hit::ReverseBtn ? Layer::Reverse : Layer::Mode;
            latched = latched == l ? Layer::Main : l;
            longPressHandled = true;
        }
        else if (pressedHit == Hit::Footswitch && held > 420.0)
        {
            longPressHandled = true;
            const bool freezeMode = proc.value (PID::fsHoldMode) > 0.5f;
            if (freezeMode)
            {
                if (proc.value (PID::bypass) < 0.5f) { fsFreezeHeld = true; setParam (PID::freeze, 1.0f); }
            }
            else
            {
                presetScrollMode = ! presetScrollMode;
                presetScrollUntil = nowMs() + 6000.0;
            }
        }
        else if (pressedHit == Hit::PresetBtn && held > 2000.0)
        {
            longPressHandled = true;
            if (! copyMode)
            {
                copyMode = true;                                      // copy current settings
                const int c = proc.currentPreset.load();
                copyTarget = (c >= 1 && c <= 8 && ! TexturalAudioProcessor::isLive (c)) ? c : 1;
                copyUntil = nowMs() + 12000.0;
            }
            else
            {
                proc.savePresetTo (copyTarget);                        // save into the chosen slot
                copyMode = false;
            }
        }
    }

    if (presetScrollMode && nowMs() > presetScrollUntil) presetScrollMode = false;
    if (copyMode && nowMs() > copyUntil) copyMode = false;

    // moteur logiciel : seulement si la fenêtre hôte a changé, ou au plus une fois par seconde
    const double now = nowMs();
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }

    // caches : reconstruits ici seulement (jamais dans paint)
    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f) rebuildCaches (sc);
    }

    // chien de garde : un redessin demandé n'arrive pas -> on relance le moteur de rendu et on redessine tout
    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0;
        lastPeer = nullptr;
        useSoftwareRenderer();
        repaint();
    }

    // on ne redessine une zone que si ce qu'elle affiche a vraiment changé (zéro travail au repos)
    auto changed = [] (float& drawn, float now) { if (std::abs (drawn - now) < 0.002f) return false; drawn = now; return true; };
    auto colourChanged = [] (Colour& drawn, Colour now) { if (drawn == now) return false; drawn = now; return true; };

    const Layer layer = currentLayer();

    for (int i = 0; i < 6; ++i)
        if (changed (drawnKnob[i], knobValue (layer, i)))
            askRepaint (pedalZone ({ kKnobs[i].x, kKnobs[i].y }, 72.0f));

    if (changed (drawnReverse, reverseAmount (layer)))
        askRepaint (pedalZone (kReverseBtn, kBtnR * 2.2f));

    {
        Colour c; float amt;
        modeLedState (layer, c, amt);
        const bool a = changed (drawnMode, amt);
        const bool b = colourChanged (drawnModeColour, c);
        if (a || b) askRepaint (pedalZone (kModeBtn, kBtnR * 2.2f));
    }

    if (changed (drawnLed, statusLedAmount (layer)))
        askRepaint (pedalZone (kLed, 44.0f));

    const bool pressChanged = pressedHit != drawnPressed;
    if (pressChanged)
    {
        if (pressedHit == Hit::Footswitch || drawnPressed == Hit::Footswitch) askRepaint (pedalZone (kFootswitch, kFootswitchR + 4.0f));
        if (pressedHit == Hit::PresetBtn  || drawnPressed == Hit::PresetBtn)  askRepaint (pedalZone (kPresetBtn, 18.0f));
        drawnPressed = pressedHit;
    }

    {
        const int cur = copyMode ? copyTarget : proc.currentPreset.load();
        const bool modified = ! copyMode && proc.isPresetModified();
        for (int i = 0; i < 4; ++i)
        {
            Colour c; float on;
            presetLedState (i, cur, modified, c, on);
            const bool a = changed (drawnPresetLed[i], on);
            const bool b = colourChanged (drawnPresetColour[i], c);
            if (a || b) askRepaint (pedalZone ({ kPresetLedX[i], kPresetLedY }, kPresetLedR * 2.2f));
        }
    }

    {
        String txt; Colour col; float alpha = 0.0f;
        if (! statusLineState (txt, col, alpha)) { txt.clear(); alpha = 0.0f; }
        const bool a = changed (drawnStatusAlpha, alpha);
        const bool b = colourChanged (drawnStatusColour, col);
        if (a || b || txt != drawnStatus)
        {
            drawnStatus = txt;
            const auto r = Rectangle<float> (284.0f, 695.0f, 521.0f, 36.0f).expanded (8.0f)
                               .transformedBy (pedalTransform().scaled (scale()));
            askRepaint (r.getSmallestIntegerContainer().expanded (2));
        }
    }
}

// zone de la fenêtre (pixels) couvrant un disque de rayon r donné dans l'espace de la pédale
Rectangle<int> TexturalAudioProcessorEditor::pedalZone (Point<float> c, float r) const
{
    return Rectangle<float> (c.x - r, c.y - r, r * 2.0f, r * 2.0f)
               .transformedBy (pedalTransform().scaled (scale()))
               .getSmallestIntegerContainer().expanded (2);
}

float TexturalAudioProcessorEditor::knobValue (Layer layer, int i) const
{
    const auto id = knobParamID (layer, i);
    return id.isNotEmpty() ? proc.param (id)->getValue() : 0.5f;
}

float TexturalAudioProcessorEditor::reverseAmount (Layer layer) const
{
    float amt = proc.value (PID::reverse) > 0.5f ? 0.85f : 0.0f;
    if (layer == Layer::Reverse) amt = blink (2.5) ? 1.0f : 0.35f;
    return amt;
}

void TexturalAudioProcessorEditor::modeLedState (Layer layer, Colour& c, float& amt) const
{
    // MODE button (LED takes the colour of the active mode symbol, flashes when frozen)
    const int m = jlimit (0, 2, roundToInt (proc.value (PID::mode)));
    c = kModeColours[m];
    amt = 0.9f;
    if (proc.value (PID::bypass) < 0.5f && proc.isFrozen()) amt = blink (3.0) ? 1.0f : 0.15f;
    if (layer == Layer::Mode) { c = Colour (0xff6dff6d); amt = blink (2.5) ? 1.0f : 0.4f; }
}

float TexturalAudioProcessorEditor::statusLedAmount (Layer layer) const
{
    const float soft = 0.55f + 0.45f * (float) std::sin (phase * MathConstants<double>::twoPi * 1.2);
    float on = proc.value (PID::bypass) < 0.5f ? 1.0f : 0.0f;
    if (presetScrollMode) on = soft;
    if (layer == Layer::Foot) on = blink (2.5) ? 1.0f : 0.2f;
    return on;
}

void TexturalAudioProcessorEditor::presetLedState (int i, int cur, bool modified, Colour& c, float& on) const
{
    // preset LEDs: 1-4 green, 5-8 red
    on = 0.0f;
    c = kPresetGreen;
    if (cur >= 1 && cur <= 8 && ((cur - 1) % 4) == i)
    {
        c = cur <= 4 ? kPresetGreen : kPresetRed;
        on = 1.0f;
        if (copyMode)       on = blink (6.0) ? 1.0f : 0.0f;
        else if (modified)  on = blink (2.0) ? 1.0f : 0.15f;
    }
}

//==============================================================================
void TexturalAudioProcessorEditor::showMenu (int learnIdx)
{
    PopupMenu m;
    const int h = getHeight();
    const int lt = proc.learnTarget.load();
    auto ccText = [this] (int i)
    {
        const int cc = proc.ccAssign[i].load();
        return cc >= 0 ? "CC " + String (cc) : String ("none");
    };
    // clic droit sur une commande : MIDI Learn de cette commande en tête du menu
    if (learnIdx >= 0)
    {
        const String name = TexturalAudioProcessor::learnName (learnIdx);
        m.addSectionHeader (name);
        m.addItem (1000 + learnIdx, "MIDI Learn: " + name + " (" + ccText (learnIdx) + ")", true, lt == learnIdx);
        m.addItem (1100 + learnIdx, "Clear MIDI: " + name, proc.ccAssign[learnIdx].load() >= 0);
        if (lt >= 0) m.addItem (1200, "Cancel MIDI learn");
        m.addSeparator();
    }
    m.addSectionHeader ("Textural (Studio Wiga)");
    m.addItem (1, "60 % (default)", true, h == kDefaultH);
    m.addItem (3, "75 %",  true, h == kH75);
    m.addItem (4, "100 %", true, h == kH100);
    m.addSeparator();

    PopupMenu save;
    for (int i = 1; i <= 8; ++i) if (! TexturalAudioProcessor::isLive (i)) save.addItem (10 + i, proc.presetName (i));
    m.addSubMenu ("Save current settings to preset", save);
    {
        const int c = proc.currentPreset.load();
        m.addItem (21, "Reset current preset to factory", c >= 1 && c <= 8 && ! TexturalAudioProcessor::isLive (c));
    }
    m.addItem (22, "Reset all presets to factory");
    m.addSeparator();
    for (int x = TexturalAudioProcessor::kExtraPreset; x <= TexturalAudioProcessor::kLastPreset; ++x)
        m.addItem (23 + x - TexturalAudioProcessor::kExtraPreset, "Load preset: " + proc.presetName (x), true,
                   proc.currentPreset.load() == x);
    m.addSeparator();

    PopupMenu midi;
    for (int i = 0; i < TexturalAudioProcessor::kNumLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (TexturalAudioProcessor::learnName (i)) + " (" + ccText (i) + ")", true, lt == i);
    midi.addSeparator();
    if (lt >= 0) midi.addItem (1200, "Cancel MIDI learn");
    midi.addItem (1201, "Reset MIDI to defaults (fixed CC map)");
    midi.addSeparator();
    midi.addItem (1202, "Program Change 1-9 = presets", false);
    m.addSubMenu ("MIDI", midi);

    SafePointer<TexturalAudioProcessorEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withMousePosition(), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& ed = *safe;
        switch (r)
        {
            case 1: ed.setHeightKeepingAspect (kDefaultH); break;
            case 2: ed.setHeightKeepingAspect (kH60);  break;
            case 3: ed.setHeightKeepingAspect (kH75);  break;
            case 4: ed.setHeightKeepingAspect (kH100); break;
            case 21: ed.proc.resetPresetToFactory (ed.proc.currentPreset.load()); break;
            case 22: ed.proc.resetAllPresets(); break;
            case 23:
            {
                const int x = TexturalAudioProcessor::kExtraPreset + r - 23;
                ed.proc.selectPreset (x);
                ed.showMessage (ed.proc.presetName (x).toUpperCase(), Colour (0xff121212));
                break;
            }
            default:
                if (r > 10 && r <= 18) ed.proc.savePresetTo (r - 10);
                else if (r >= 1000 && r < 1000 + TexturalAudioProcessor::kNumLearn)        // re-cliquer annule
                    ed.proc.learnTarget.store (ed.proc.learnTarget.load() == r - 1000 ? -1 : r - 1000);
                else if (r >= 1100 && r < 1100 + TexturalAudioProcessor::kNumLearn) ed.proc.ccAssign[r - 1100].store (-1);
                else if (r == 1200) ed.proc.learnTarget.store (-1);
                else if (r == 1201) { ed.proc.learnTarget.store (-1); ed.proc.resetMidiMap(); }
                break;
        }
    });
}

//==============================================================================
//  Painting
//==============================================================================
void TexturalAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::black);

    if (bgCache.isValid())
    {
        // anti-gel : fond + façade déjà composés à la taille de la fenêtre par le timer (aucun rescale ici)
        g.setImageResamplingQuality (Graphics::lowResamplingQuality);
        g.drawImage (bgCache, getLocalBounds().toFloat());
        g.addTransform (AffineTransform::scale (scale()));
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.addTransform (pedalTransform());      // everything below belongs to the pedal
    }
    else
    {
        g.addTransform (AffineTransform::scale (scale()));
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        // black backdrop with a faint echo of the pedal's granular wave pattern
        g.drawImage (backgroundImg, Rectangle<float> (0, 0, kDesignW, kDesignH), RectanglePlacement::fillDestination);
        g.addTransform (pedalTransform());          // everything below belongs to the pedal
        g.drawImage (faceplate, Rectangle<float> (kPedalX, 0, kPedalSpaceW - 2.0f * kPedalX, kDesignH));
    }

    const Layer layer = currentLayer();

    // knobs
    for (int i = 0; i < 6; ++i)
    {
        const float v = knobValue (layer, i);
        const float angle = degreesToRadians (-135.0f + 270.0f * v);
        drawKnob (g, { kKnobs[i].x, kKnobs[i].y }, kKnobs[i].r, angle, kKnobs[i].orange);
    }

    // REVERSE button
    drawRoundButton (g, kReverseBtn, kBtnR, kReverseColour, reverseAmount (layer));

    // MODE button (LED takes the colour of the active mode symbol, flashes when frozen)
    {
        Colour c; float amt;
        modeLedState (layer, c, amt);
        drawRoundButton (g, kModeBtn, kBtnR, c, amt);
    }

    // status LED above the footswitch
    drawStatusLed (g, kLed, statusLedAmount (layer));

    drawFootswitch (g, kFootswitch, pressedHit == Hit::Footswitch);

    // preset LEDs: 1-4 green, 5-8 red
    {
        const int cur = copyMode ? copyTarget : proc.currentPreset.load();
        const bool modified = ! copyMode && proc.isPresetModified();
        for (int i = 0; i < 4; ++i)
        {
            Colour c; float on;
            presetLedState (i, cur, modified, c, on);
            drawPresetLed (g, { kPresetLedX[i], kPresetLedY }, c, on);
        }
        drawPresetButton (g, kPresetBtn, pressedHit == Hit::PresetBtn);
    }

    drawStatusLine (g);
}

bool TexturalAudioProcessorEditor::statusLineState (String& txt, Colour& col, float& alpha) const
{
    // nothing is shown until a knob is turned (or a button pressed); only that setting is displayed
    const bool isMsg = statusMsg.isNotEmpty();
    if (! isMsg && (statusKnob < 0 || statusParam.isEmpty())) return false;
    const double age = nowMs() - statusMs;
    const bool turning = dragKnob >= 0 && dragKnob == statusKnob;
    alpha = 1.0f;
    if (! turning)
    {
        if (age > 1600.0) return false;
        if (age > 1100.0) alpha = (float) (1.0 - (age - 1100.0) / 500.0);
    }

    txt = isMsg ? statusMsg : proc.describe (statusParam);

    // text takes the colour of the knob being turned
    col = isMsg ? statusMsgColour
                : (kKnobs[statusKnob].orange ? Colour (0xfff26d0f) : Colour (0xff121212));
    return true;
}

void TexturalAudioProcessorEditor::drawStatusLine (Graphics& g) const
{
    String txt; Colour col; float alpha = 1.0f;
    if (! statusLineState (txt, col, alpha)) return;
    g.setColour (col.withAlpha (alpha));
    g.setFont (FontOptions (24.0f, Font::bold));
    g.drawFittedText (txt, Rectangle<int> (284, 695, 521, 36), Justification::centred, 1, 0.75f);
}

//==============================================================================
void TexturalAudioProcessorEditor::drawKnob (Graphics& g, Point<float> c, float, float angle, bool orange) const
{
    // Knob photos supplied by the user. The base never turns; the 7-lobe cap turns with
    // the value while its highlights stay where the light is; the pointer turns freely.
    const int k = orange ? 1 : 0;
    constexpr float spriteScale = 62.0f / 132.0f;            // sprite base radius 132 px -> 62 design units
    const float half = (float) kKnobFramePx * spriteScale * 0.5f;
    const auto place = AffineTransform::scale (spriteScale).translated (c.x - half, c.y - half);

    g.setOpacity (1.0f);
    g.drawImageTransformed (knobBase[k], place);

    const float period = 360.0f / 7.0f;
    float m = std::fmod (radiansToDegrees (angle), period); if (m < 0) m += period;
    const int frame = roundToInt (m / period * (float) kKnobFrames) % kKnobFrames;
    g.drawImageTransformed (knobFrames[k][frame], place);

    // pointer sprite: photo pixels -> design units (photo base radius 385 px -> 62 units)
    constexpr float photoToDesign = 62.0f / 385.0f;
    const Point<float> off = orange ? Point<float> (-25.0f, -343.0f) : Point<float> (-31.0f, -335.0f);
    g.drawImageTransformed (knobPointer[k], AffineTransform::scale (photoToDesign)
                                                .translated (off * photoToDesign)
                                                .rotated (angle)
                                                .translated (c));
}

void TexturalAudioProcessorEditor::drawLedLens (Graphics& g, Point<float> c, float r, Colour col, float on, bool diffused) const
{
    // Realistic domed LED lens: coloured body, hot core over the die, dark refractive rim,
    // fixed specular reflection (light from top-left) and a short bloom when lit.
    if (on > 0.01f)
    {
        ColourGradient bloom (col.withAlpha (0.62f * on), c.x, c.y, col.withAlpha (0.0f), c.x + r * 2.1f, c.y, true);
        g.setGradientFill (bloom);
        g.fillEllipse (c.x - r * 2.1f, c.y - r * 2.1f, r * 4.2f, r * 4.2f);
    }

    const Colour offBody = diffused ? Colour (0xffdcd7ca) : col.darker (2.4f).withMultipliedSaturation (0.7f);
    const Colour body    = offBody.interpolatedWith (col, on);
    const Colour rim     = (diffused ? Colour (0xff8f8878) : col.darker (3.2f)).interpolatedWith (col.darker (0.9f), on);
    ColourGradient lens (body.brighter (0.15f * (1.0f - on)).withMultipliedSaturation (1.0f + 0.3f * on), c.x - r * 0.1f, c.y - r * 0.15f, rim, c.x + r, c.y + r * 0.2f, true);
    g.setGradientFill (lens);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

    // emitter die / hot core
    if (on > 0.01f)
    {
        ColourGradient core (Colours::white.withAlpha (0.9f * on), c.x, c.y,
                             col.withAlpha (0.0f), c.x + r * 0.42f, c.y, true);
        g.setGradientFill (core);
        g.fillEllipse (c.x - r * 0.42f, c.y - r * 0.42f, r * 0.84f, r * 0.84f);
    }
    g.setColour ((on > 0.5f ? Colours::white : Colours::black).withAlpha (diffused ? 0.10f : 0.22f));
    g.fillRect (Rectangle<float> (r * 0.22f, r * 0.22f).withCentre (c));

    // refractive inner ring and outer edge
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawEllipse (c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f, r * 0.06f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, jmax (0.8f, r * 0.06f));

    // specular reflection of the room light (fixed)
    ColourGradient spec (Colours::white.withAlpha (0.85f), c.x - r * 0.35f, c.y - r * 0.55f,
                         Colours::white.withAlpha (0.0f), c.x - r * 0.35f + r * 0.5f, c.y - r * 0.55f, true);
    g.setGradientFill (spec);
    g.fillEllipse (c.x - r * 0.78f, c.y - r * 0.85f, r * 0.85f, r * 0.55f);
    g.setColour (Colours::white.withAlpha (0.35f));
    Path lower;
    lower.addCentredArc (c.x, c.y, r * 0.80f, r * 0.80f, 0.0f, degreesToRadians (110.0f), degreesToRadians (160.0f), true);
    g.strokePath (lower, PathStrokeType (r * 0.08f, PathStrokeType::curved, PathStrokeType::rounded));
}

void TexturalAudioProcessorEditor::drawRoundButton (Graphics& g, Point<float> c, float r, Colour lit, float amount) const
{
    // chrome bezel around an illuminated push-button lens
    const float br = r * 1.22f;
    ColourGradient bezel (Colour (0xfff7f7f8), c.x - br * 0.6f, c.y - br * 0.8f, Colour (0xff6f6f74), c.x + br * 0.6f, c.y + br * 0.8f, false);
    bezel.addColour (0.5, Colour (0xffbdbdc2));
    g.setGradientFill (bezel);
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (Colour (0xff4c4c50));
    g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 0.9f);
    drawLedLens (g, c, r, lit, amount, true);
}

void TexturalAudioProcessorEditor::drawStatusLed (Graphics& g, Point<float> c, float on) const
{
    // the black rubber bezel comes from the photo; only the lens is drawn
    drawLedLens (g, c, 20.0f, Colour (0xff1e90ff), on, false);
}

void TexturalAudioProcessorEditor::drawFootswitch (Graphics& g, Point<float> c, bool pressed) const
{
    // The footswitch is the one from the original visual (already in the faceplate).
    // When pressed, the same photo is redrawn slightly sunk and darker.
    if (! pressed || ! footswitchImg.isValid()) return;
    const float r = kFootswitchR * 0.975f;
    const Rectangle<float> dst (c.x - r, c.y - r + 1.8f, r * 2.0f, r * 2.0f);
    g.setOpacity (1.0f);
    g.drawImage (footswitchImg, dst);
    g.setColour (Colours::black.withAlpha (0.16f));
    g.fillEllipse (dst.reduced (r * 0.42f));
}

void TexturalAudioProcessorEditor::drawPresetLed (Graphics& g, Point<float> c, Colour col, float on) const
{
    drawLedLens (g, c, kPresetLedR, col, on, true);
}

void TexturalAudioProcessorEditor::drawPresetButton (Graphics& g, Point<float> c, bool pressed) const
{
    const float r = pressed ? 14.0f : 15.0f;
    ColourGradient gr (Colour (0xff4a4a4c), c.x - r * 0.3f, c.y - r * 0.5f, Colour (0xff030303), c.x + r * 0.6f, c.y + r * 0.8f, true);
    g.setGradientFill (gr);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colours::black);
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.2f);
    g.setColour (Colours::white.withAlpha (0.28f));
    g.fillEllipse (c.x - r * 0.5f, c.y - r * 0.72f, r * 0.75f, r * 0.4f);
}
