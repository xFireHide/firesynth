#pragma once

#include <array>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "ui/SynthLookAndFeel.h"
#include "ui/PadComponent.h"
#include "ui/FxPadComponent.h"
#include "ui/WheelComponent.h"

/**
    GUI — runs only on the message thread. Controls talk to the audio engine
    through the APVTS (atomic parameter writes), never touching the audio thread.

    Note: hardware knob mapping is intentionally disabled (see README) because the
    AMW Mini 32p's 4 knobs and pitch wheel all emit an identical Pitch Bend. The
    on-screen Pitch/Mod wheels mirror that incoming Pitch Bend / Mod (CC1) value.
*/
class PianoSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public  juce::DragAndDropContainer,
                                       private juce::Timer
{
public:
    explicit PianoSynthAudioProcessorEditor (PianoSynthAudioProcessor&);
    ~PianoSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void parentHierarchyChanged() override; // clears the OS window title (Standalone)

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider, gainSlider;
    juce::Slider detuneSlider, oscMixSlider, cutoffSlider, resonanceSlider;
    juce::Slider driveSlider, reverbSlider;
    juce::ComboBox waveformBox, presetBox;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel, gainLabel;
    juce::Label detuneLabel, oscMixLabel, cutoffLabel, resonanceLabel;
    juce::Label driveLabel, reverbLabel;

    std::unique_ptr<APVTS::SliderAttachment>   attackAtt, decayAtt, sustainAtt, releaseAtt, gainAtt;
    std::unique_ptr<APVTS::SliderAttachment>   detuneAtt, oscMixAtt, cutoffAtt, resonanceAtt;
    std::unique_ptr<APVTS::SliderAttachment>   driveAtt, reverbAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> waveformAtt;

    void applyPreset (int presetIndex);

    // On-screen keyboard (play without hardware MIDI).
    juce::MidiKeyboardComponent keyboard;

    // Pitch / Modulation wheels (like the AMW Mini 32p). Pitch mirrors hardware
    // Pitch Bend (display only); Mod mirrors + sets the mod wheel (vibrato).
    WheelComponent pitchWheelUI, modWheelUI;

    // Per-knob target selectors (assign each hardware knob CC to a parameter).
    std::array<juce::ComboBox, PianoSynthAudioProcessor::kNumMacros> knobAssignBox;
    juce::Label knobAssignTitle;

    // 16 sample pads (drag & drop audio + click to play). Each pad doubles as a
    // voice-FX pad: drop an effect chip on it and click toggles the effect on the mic.
    std::array<PadComponent, PadEngine::kNumPads> pads;
    juce::Label padsTitle;

    // Voice FX: a palette of draggable effect chips. Drag a chip onto one of the
    // pads above to assign it; clicking that pad applies the effect to the mic.
    std::array<FxChip, VoiceFX::kNumFx> fxChips;
    juce::Label fxTitle, fxHint;

    // Retorno (monitor) da voz: liga ouvir o microfone + knob de volume "Voz".
    juce::ToggleButton voiceMonitorButton;
    juce::Slider       voiceLevelSlider;
    juce::Label        voiceLevelLabel;
    std::unique_ptr<APVTS::ButtonAttachment> voiceMonitorAtt;
    std::unique_ptr<APVTS::SliderAttachment> voiceLevelAtt;

    // MIDI activity indicator (LED + last note name).
    juce::Label midiActivityLabel;
    juce::Rectangle<int> ledBounds;
    int   lastSeenNoteCount = 0;
    float ledBrightness     = 0.0f;

    SynthLookAndFeel lookAndFeel;
    PianoSynthAudioProcessor& processorRef;

    void configureRotary (juce::Slider&, juce::Label&, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoSynthAudioProcessorEditor)
};
