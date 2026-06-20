#include "PluginEditor.h"

// Parameters a hardware knob can be assigned to (Master is excluded — that is the
// volume fader / CC7). Pairs of { display name, APVTS parameter id }. "Off" = unmapped.
static const std::array<std::pair<juce::String, juce::String>, 11> knobTargets = {{
    { "Off",       "" },
    { "Cutoff",    "cutoff" },
    { "Resonance", "resonance" },
    { "Drive",     "drive" },
    { "Reverb",    "reverb" },
    { "Osc Mix",   "oscMix" },
    { "Detune",    "detune" },
    { "Attack",    "attack" },
    { "Decay",     "decay" },
    { "Sustain",   "sustain" },
    { "Release",   "release" },
}};

PianoSynthAudioProcessorEditor::PianoSynthAudioProcessorEditor (PianoSynthAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    configureRotary (attackSlider,    attackLabel,    "Attack");
    configureRotary (decaySlider,     decayLabel,     "Decay");
    configureRotary (sustainSlider,   sustainLabel,   "Sustain");
    configureRotary (releaseSlider,   releaseLabel,   "Release");
    configureRotary (cutoffSlider,    cutoffLabel,    "Cutoff");
    configureRotary (resonanceSlider, resonanceLabel, "Reso");
    configureRotary (detuneSlider,    detuneLabel,    "Detune");
    configureRotary (oscMixSlider,    oscMixLabel,    "Osc Mix");
    configureRotary (driveSlider,     driveLabel,     "Drive");
    configureRotary (reverbSlider,    reverbLabel,    "Reverb");
    configureRotary (gainSlider,      gainLabel,      "Master");

    waveformBox.addItemList ({ "Sine", "Saw", "Square", "Triangle" }, 1);
    addAndMakeVisible (waveformBox);

    presetBox.addItemList ({ "Organ", "Synth Bass", "Lead", "Pad", "Pluck" }, 1);
    presetBox.setTextWhenNothingSelected ("Presets...");
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    keyboard.setVelocity (0.85f, false); // fixed velocity: clicking anywhere on a key sounds the same
    addAndMakeVisible (keyboard);

    // Pitch / Modulation wheels (visual like the keyboard).
    pitchWheelUI.setLabel ("Pitch");
    pitchWheelUI.setValueExternally (0.5f); // pitch rests at centre
    // Display-only: knob mapping is disabled (see README), so dragging does nothing.
    addAndMakeVisible (pitchWheelUI);

    modWheelUI.setLabel ("Mod");
    modWheelUI.onValueChange = [this] (float v)
    {
        processorRef.modWheelValue.store (v, std::memory_order_relaxed); // vibrato depth
    };
    addAndMakeVisible (modWheelUI);

    midiActivityLabel.setText ("MIDI: waiting...", juce::dontSendNotification);
    midiActivityLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (midiActivityLabel);

    // Knob assignment: one selector per hardware knob (CC 20-23). Choosing a target
    // re-routes that knob. targetParamID is read/written only on the message thread.
    knobAssignTitle.setText ("KNOBS (CC 20-23) ->", juce::dontSendNotification);
    knobAssignTitle.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (knobAssignTitle);

    const int defaultId[PianoSynthAudioProcessor::kNumMacros] = { 2, 3, 4, 5 }; // Cutoff, Reso, Drive, Reverb
    for (int i = 0; i < PianoSynthAudioProcessor::kNumMacros; ++i)
    {
        auto& cb = knobAssignBox[(size_t) i];
        for (int j = 0; j < (int) knobTargets.size(); ++j)
            cb.addItem (knobTargets[(size_t) j].first, j + 1);

        cb.setSelectedId (defaultId[i], juce::dontSendNotification);
        processorRef.macros[(size_t) i].targetParamID = knobTargets[(size_t) (defaultId[i] - 1)].second;

        cb.onChange = [this, i]
        {
            const int id = knobAssignBox[(size_t) i].getSelectedId();
            if (id > 0)
                processorRef.macros[(size_t) i].targetParamID = knobTargets[(size_t) (id - 1)].second;
        };
        addAndMakeVisible (cb);
    }

    // 8 pads: drag an audio file on a pad; click (or hardware pad) to play it.
    padsTitle.setText ("PADS", juce::dontSendNotification);
    padsTitle.setJustificationType (juce::Justification::centredLeft);
    padsTitle.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (padsTitle);

    for (int i = 0; i < PadEngine::kNumPads; ++i)
    {
        auto& pad = pads[(size_t) i];
        const int note = PianoSynthAudioProcessor::kPadBaseNote + i;
        pad.setIndexInfo (i + 1, juce::MidiMessage::getMidiNoteName (note, true, true, 3));
        pad.setFileName (processorRef.padEngine.getName (i));

        pad.onTrigger = [this, i] { processorRef.padEngine.trigger (i); };
        pad.onFileDropped = [this, i] (const juce::File& file)
        {
            juce::String error;
            if (processorRef.padEngine.loadFile (i, file, error))
                pads[(size_t) i].setFileName (file.getFileName());
            else
                pads[(size_t) i].setFileName (error);
        };
        addAndMakeVisible (pad);
    }

    // Attachments: GUI <-> APVTS (atomic writes; never touch the audio thread).
    auto& apvts = processorRef.apvts;
    attackAtt    = std::make_unique<APVTS::SliderAttachment>   (apvts, "attack",    attackSlider);
    decayAtt     = std::make_unique<APVTS::SliderAttachment>   (apvts, "decay",     decaySlider);
    sustainAtt   = std::make_unique<APVTS::SliderAttachment>   (apvts, "sustain",   sustainSlider);
    releaseAtt   = std::make_unique<APVTS::SliderAttachment>   (apvts, "release",   releaseSlider);
    cutoffAtt    = std::make_unique<APVTS::SliderAttachment>   (apvts, "cutoff",    cutoffSlider);
    resonanceAtt = std::make_unique<APVTS::SliderAttachment>   (apvts, "resonance", resonanceSlider);
    detuneAtt    = std::make_unique<APVTS::SliderAttachment>   (apvts, "detune",    detuneSlider);
    oscMixAtt    = std::make_unique<APVTS::SliderAttachment>   (apvts, "oscMix",    oscMixSlider);
    driveAtt     = std::make_unique<APVTS::SliderAttachment>   (apvts, "drive",     driveSlider);
    reverbAtt    = std::make_unique<APVTS::SliderAttachment>   (apvts, "reverb",    reverbSlider);
    gainAtt      = std::make_unique<APVTS::SliderAttachment>   (apvts, "gain",      gainSlider);
    waveformAtt  = std::make_unique<APVTS::ComboBoxAttachment> (apvts, "waveform",  waveformBox);

    setSize (780, 600);
    startTimerHz (30); // MIDI LED + wheels mirror hardware
}

PianoSynthAudioProcessorEditor::~PianoSynthAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PianoSynthAudioProcessorEditor::parentHierarchyChanged()
{
    // In Standalone the window is a DocumentWindow whose title = component name.
    if (auto* top = getTopLevelComponent())
        top->setName ({});
}

// Sets a whole sound at once by writing to the APVTS parameters.
void PianoSynthAudioProcessorEditor::applyPreset (int presetIndex)
{
    if (presetIndex < 0)
        return;

    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* param = processorRef.apvts.getParameter (id))
            param->setValueNotifyingHost (
                processorRef.apvts.getParameterRange (id).convertTo0to1 (value));
    };

    // wave: 0=Sine 1=Saw 2=Square 3=Triangle
    struct Preset { float wave, atk, dec, sus, rel, det, mix, cut, res, gain; };
    static const Preset presets[] = {
        // Organ        wave atk    dec   sus   rel   det   mix   cut     res   gain
        {                0, 0.005f, 0.10f, 1.0f, 0.15f, 8.0f, 0.5f, 20000.f, 0.7f, 0.70f },
        // Synth Bass
        {                1, 0.005f, 0.12f, 0.6f, 0.10f, 0.0f, 0.0f,   600.f, 2.5f, 0.80f },
        // Lead
        {                1, 0.010f, 0.20f, 0.8f, 0.25f, 12.f, 0.5f,  3000.f, 3.0f, 0.65f },
        // Pad
        {                3, 0.800f, 0.40f, 0.8f, 1.20f, 18.f, 0.5f,  1800.f, 1.2f, 0.60f },
        // Pluck
        {                2, 0.002f, 0.18f, 0.0f, 0.12f, 0.0f, 0.0f,  2500.f, 1.5f, 0.70f },
    };

    if (presetIndex >= (int) std::size (presets))
        return;

    const auto& pr = presets[presetIndex];
    set ("waveform",  pr.wave);
    set ("attack",    pr.atk);
    set ("decay",     pr.dec);
    set ("sustain",   pr.sus);
    set ("release",   pr.rel);
    set ("detune",    pr.det);
    set ("oscMix",    pr.mix);
    set ("cutoff",    pr.cut);
    set ("resonance", pr.res);
    set ("gain",      pr.gain);
}

void PianoSynthAudioProcessorEditor::timerCallback()
{
    // MIDI Volume (CC 7) -> Master Gain (moves the slider too).
    const float vol = processorRef.pendingMidiVolume.exchange (-1.0f, std::memory_order_relaxed);
    if (vol >= 0.0f)
        if (auto* gainParam = processorRef.apvts.getParameter ("gain"))
            gainParam->setValueNotifyingHost (vol);

    // Pitch wheel UI mirrors incoming Pitch Bend (knobs/pitch wheel) - no param mapping.
    const float kv = processorRef.knobValue01.exchange (-1.0f, std::memory_order_relaxed);
    if (kv >= 0.0f)
        pitchWheelUI.setValueExternally (kv);

    // Mod wheel UI mirrors the hardware mod wheel (CC1).
    modWheelUI.setValueExternally (processorRef.modWheelValue.load (std::memory_order_relaxed));

    // Hardware knobs (CC 20-23) -> mapped parameters. Applied on the message thread.
    for (int i = 0; i < PianoSynthAudioProcessor::kNumMacros; ++i)
    {
        const float v = processorRef.macros[(size_t) i].pending.exchange (-1.0f, std::memory_order_relaxed);
        if (v >= 0.0f)
            if (auto* param = processorRef.apvts.getParameter (processorRef.macros[(size_t) i].targetParamID))
                param->setValueNotifyingHost (v);
    }

    // Pads light up when triggered (click already flashes; MIDI via consumeActivity).
    for (int i = 0; i < PadEngine::kNumPads; ++i)
    {
        if (processorRef.padEngine.consumeActivity (i))
            pads[(size_t) i].flash();
        pads[(size_t) i].decay();
    }

    // MIDI activity LED + last note name.
    const int count = processorRef.midiNoteOnCount.load (std::memory_order_relaxed);
    if (count != lastSeenNoteCount)
    {
        lastSeenNoteCount = count;
        ledBrightness = 1.0f;
        const int note = processorRef.lastNoteNumber.load (std::memory_order_relaxed);
        midiActivityLabel.setText ("MIDI in: " + juce::MidiMessage::getMidiNoteName (note, true, true, 3),
                                   juce::dontSendNotification);
    }
    else
    {
        ledBrightness = juce::jmax (0.0f, ledBrightness - 0.08f);
    }

    repaint (ledBounds.expanded (2));
}

void PianoSynthAudioProcessorEditor::configureRotary (juce::Slider& slider,
                                                      juce::Label& label,
                                                      const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (SynthLookAndFeel::textCol));
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (SynthLookAndFeel::textCol));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void PianoSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (SynthLookAndFeel::bg));

    const auto off = juce::Colour (0xff303030);
    const auto on  = juce::Colour (SynthLookAndFeel::accent);
    g.setColour (off.interpolatedWith (on, ledBrightness));
    g.fillEllipse (ledBounds.toFloat());
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (ledBounds.toFloat(), 1.0f);
}

void PianoSynthAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10, 10);

    // Status row: LED + last note (left) | Preset/Waveform combos (right).
    auto statusRow = area.removeFromTop (26);
    ledBounds = statusRow.removeFromLeft (16).withSizeKeepingCentre (12, 12);
    statusRow.removeFromLeft (6);
    midiActivityLabel.setBounds (statusRow.removeFromLeft (170));

    auto combos = statusRow;
    const int comboW = juce::jmin (160, combos.getWidth() / 2 - 8);
    waveformBox.setBounds (combos.removeFromRight (comboW));
    combos.removeFromRight (8);
    presetBox.setBounds (combos.removeFromRight (comboW));

    area.removeFromTop (10);

    // Bottom: pitch/mod wheels (left) + keyboard.
    {
        auto bottomRow = area.removeFromBottom (84);
        auto wheelArea = bottomRow.removeFromLeft (94);
        pitchWheelUI.setBounds (wheelArea.removeFromLeft (47).reduced (3, 0));
        modWheelUI.setBounds (wheelArea.reduced (3, 0));
        bottomRow.removeFromLeft (8);
        keyboard.setBounds (bottomRow);
    }
    area.removeFromBottom (10);

    // Knob-assignment row (title + 4 selectors), just above the pads.
    {
        auto knobRow = area.removeFromBottom (26);
        knobAssignTitle.setBounds (knobRow.removeFromLeft (150));
        const int n = PianoSynthAudioProcessor::kNumMacros;
        const int cw = knobRow.getWidth() / n;
        for (int i = 0; i < n; ++i)
            knobAssignBox[(size_t) i].setBounds (knobRow.removeFromLeft (cw).reduced (3, 1));
    }
    area.removeFromBottom (8);

    // Pads: title + 2x4 grid.
    auto padsBlock = area.removeFromBottom (170);
    padsTitle.setBounds (padsBlock.removeFromTop (18));
    {
        const int padCols = 8, padRows = 2; // 8 per bank: row 0 = A (1-8), row 1 = B (9-16)
        const int cellW = padsBlock.getWidth() / padCols;
        const int cellH = padsBlock.getHeight() / padRows;
        for (int i = 0; i < PadEngine::kNumPads; ++i)
        {
            const int col = i % padCols;
            const int row = i / padCols;
            pads[(size_t) i].setBounds (juce::Rectangle<int> (padsBlock.getX() + col * cellW,
                                                              padsBlock.getY() + row * cellH,
                                                              cellW, cellH).reduced (4));
        }
    }
    area.removeFromBottom (10);

    // Synth knobs: 2 rows x 5 columns (uses the remaining top space).
    juce::Slider* row1[] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider, &cutoffSlider, &resonanceSlider };
    juce::Label*  lab1[] = { &attackLabel,  &decayLabel,  &sustainLabel,  &releaseLabel,  &cutoffLabel,  &resonanceLabel  };
    juce::Slider* row2[] = { &detuneSlider, &oscMixSlider, &driveSlider, &reverbSlider, &gainSlider, nullptr };
    juce::Label*  lab2[] = { &detuneLabel,  &oscMixLabel,  &driveLabel,  &reverbLabel,  &gainLabel,  nullptr };

    const int cols = 6;
    const int rowH = area.getHeight() / 2;
    auto top = area.removeFromTop (rowH);
    auto bottom = area;

    auto layRow = [] (juce::Rectangle<int> r, juce::Slider** sl, juce::Label** lb, int numCols)
    {
        const int cellW = r.getWidth() / numCols;
        for (int i = 0; i < numCols; ++i)
        {
            auto cell = r.removeFromLeft (cellW).reduced (4);
            if (sl[i] == nullptr) continue;
            lb[i]->setBounds (cell.removeFromTop (18));
            sl[i]->setBounds (cell);
        }
    };

    layRow (top, row1, lab1, cols);
    layRow (bottom, row2, lab2, cols);
}
