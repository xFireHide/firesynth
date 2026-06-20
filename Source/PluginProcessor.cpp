#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/SynthVoice.h"
#include "dsp/SynthSound.h"

#include <cmath>

namespace ParamID
{
    constexpr auto waveform = "waveform";
    constexpr auto attack   = "attack";
    constexpr auto decay    = "decay";
    constexpr auto sustain  = "sustain";
    constexpr auto release  = "release";
    constexpr auto detune   = "detune";
    constexpr auto oscMix   = "oscMix";
    constexpr auto cutoff   = "cutoff";
    constexpr auto resonance = "resonance";
    constexpr auto drive    = "drive";
    constexpr auto reverb   = "reverb";
    constexpr auto gain     = "gain";
}

PianoSynthAudioProcessor::PianoSynthAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Cache do ponteiro atomico do ganho master (lido na thread de audio).
    gainParam   = apvts.getRawParameterValue (ParamID::gain);
    cutoffParam = apvts.getRawParameterValue (ParamID::cutoff);
    resoParam   = apvts.getRawParameterValue (ParamID::resonance);
    driveParam  = apvts.getRawParameterValue (ParamID::drive);
    reverbParam = apvts.getRawParameterValue (ParamID::reverb);

    // Coleta dos ponteiros atomicos que cada voz vai ler em tempo real.
    SynthVoice::ParameterPointers vp;
    vp.waveform = apvts.getRawParameterValue (ParamID::waveform);
    vp.attack    = apvts.getRawParameterValue (ParamID::attack);
    vp.decay     = apvts.getRawParameterValue (ParamID::decay);
    vp.sustain   = apvts.getRawParameterValue (ParamID::sustain);
    vp.release   = apvts.getRawParameterValue (ParamID::release);
    vp.detune    = apvts.getRawParameterValue (ParamID::detune);
    vp.oscMix    = apvts.getRawParameterValue (ParamID::oscMix);
    vp.cutoff    = apvts.getRawParameterValue (ParamID::cutoff);
    vp.resonance = apvts.getRawParameterValue (ParamID::resonance);
    vp.modWheel  = &modWheelValue;

    synth.addSound (new SynthSound());
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new SynthVoice (vp));

    // Knobs do controlador (AMW Mini 32 etc.) -> parametros. CC definido por MIDI Learn.
    // Default targets (the GUI can re-assign these per knob). Master is intentionally
    // NOT a knob target — it is driven by the volume fader (CC7).
    macros[0].targetParamID = ParamID::cutoff;
    macros[1].targetParamID = ParamID::resonance;
    macros[2].targetParamID = ParamID::drive;
    macros[3].targetParamID = ParamID::reverb;

    // Default mapping for the AMW Mini 32p knobs after reconfiguring them in the
    // MIDIPLUS editor to send CC 20..23 (ch1). Signature = (type<<24)|(ch<<8)|cc.
    auto ccSig = [] (int ch, int cc) { return (1 << 24) | (ch << 8) | cc; };
    macros[0].learnedSig.store (ccSig (1, 20)); // knob 1 -> Cutoff
    macros[1].learnedSig.store (ccSig (1, 21)); // knob 2 -> Resonance
    macros[2].learnedSig.store (ccSig (1, 22)); // knob 3 -> Osc Mix
    macros[3].learnedSig.store (ccSig (1, 23)); // knob 4 -> Master Gain
}

juce::AudioProcessorValueTreeState::ParameterLayout
PianoSynthAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::waveform, 1 }, "Waveform",
        StringArray { "Sine", "Saw", "Square", "Triangle" }, 0));

    // Envelopes em segundos; resposta logaritmica (skew) e mais musical nos tempos.
    const auto timeRange = NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.3f);

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::attack, 1 },  "Attack",  timeRange, 0.01f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::decay, 1 },   "Decay",   timeRange, 0.10f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::sustain, 1 }, "Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.80f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::release, 1 }, "Release", timeRange, 0.30f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::detune, 1 }, "Detune (cents)",
        NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::oscMix, 1 }, "Osc Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    // Cutoff com resposta logaritmica (skew) — mais musical no espectro audivel.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::cutoff, 1 }, "Cutoff",
        NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::resonance, 1 }, "Resonance",
        NormalisableRange<float> (0.1f, 8.0f, 0.01f, 0.5f), 0.7f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::drive, 1 }, "Drive",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::reverb, 1 }, "Reverb",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::gain, 1 }, "Master Gain",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));

    return layout;
}

bool PianoSynthAudioProcessor::classifyControl (const juce::MidiMessage& m, int& signature, float& normalised)
{
    const int ch = m.getChannel(); // 1..16 (0 se sem canal)

    if (m.isController())
    {
        signature  = (1 << 24) | (ch << 8) | m.getControllerNumber();
        normalised = static_cast<float> (m.getControllerValue()) / 127.0f;
        return true;
    }
    // Pitch Bend e tratado separadamente: e o "knob universal" (knobs do AMW Mini 32).
    if (m.isChannelPressure())
    {
        signature  = (3 << 24) | (ch << 8);
        normalised = static_cast<float> (m.getChannelPressureValue()) / 127.0f;
        return true;
    }
    return false;
}

void PianoSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    padEngine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    reverb.setSampleRate (sampleRate);
}

bool PianoSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void PianoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // O motor escreve em buffer limpo; sintetizador acumula as vozes ativas.
    buffer.clear();

    // Funde os eventos do teclado on-screen com o MIDI vindo do host/hardware.
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    // Telemetria + macros de hardware. Tudo RT-safe: so atomics, sem lock/alloc.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            midiNoteOnCount.fetch_add (1, std::memory_order_relaxed);
            lastNoteNumber.store (msg.getNoteNumber(), std::memory_order_relaxed);
        }

        // Modulation wheel (CC 1) -> vibrato global, alem do mapeamento de macros.
        if (msg.isControllerOfType (1))
            modWheelValue.store (static_cast<float> (msg.getControllerValue()) / 127.0f,
                                 std::memory_order_relaxed);

        // Volume MIDI (CC 7) -> Master Gain. So enfileira; a GUI aplica no parametro.
        if (msg.isControllerOfType (7))
            pendingMidiVolume.store (static_cast<float> (msg.getControllerValue()) / 127.0f,
                                     std::memory_order_relaxed);

        // Pitch Bend (pitch wheel): roda da GUI espelha + aplica nos pads (+/- 2 semitons).
        if (msg.isPitchWheel())
        {
            knobValue01.store (static_cast<float> (msg.getPitchWheelValue()) / 16383.0f,
                               std::memory_order_relaxed);
            padPitchBendSemis = (static_cast<float> (msg.getPitchWheelValue()) - 8192.0f) / 8192.0f * 2.0f;
        }

        int sig = 0;
        float norm = 0.0f;
        if (classifyControl (msg, sig, norm)) // CC (knobs 20-23 etc.) / Aftertouch
        {
            // Each knob CC maps to its macro's parameter; GUI applies the pending value.
            for (auto& m : macros)
                if (m.learnedSig.load (std::memory_order_relaxed) == sig)
                    m.pending.store (norm, std::memory_order_relaxed);
        }
    }

    // Knob PARAMETER mapping stays disabled (4 knobs + wheel share one Pitch Bend),
    // but the Pitch Bend is forwarded to the voices so the pitch wheel bends pitch
    // (+/- 2 semitones). Side effect: turning a knob also bends, since same signal.
    juce::MidiBuffer synthMidi;

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        const bool isPadNote = msg.getChannel() == kPadMidiChannel
                             && (msg.isNoteOn() || msg.isNoteOff())
                             && msg.getNoteNumber() >= kPadBaseNote
                             && msg.getNoteNumber() <  kPadBaseNote + PadEngine::kNumPads;

        if (isPadNote)
        {
            if (msg.isNoteOn() && msg.getVelocity() > 0)
                padEngine.trigger (msg.getNoteNumber() - kPadBaseNote);
            continue; // nao envia ao sintetizador
        }

        synthMidi.addEvent (msg, metadata.samplePosition);
    }

    // Renderiza as vozes do sintetizador e, por cima, mistura os pads tocando.
    synth.renderNextBlock (buffer, synthMidi, 0, buffer.getNumSamples());

    // Pads passam pelos mesmos efeitos do synth: pitch bend + filtro (Cutoff/Resonance).
    const float padCutoff = cutoffParam != nullptr ? cutoffParam->load() : 20000.0f;
    const float padReso   = resoParam   != nullptr ? resoParam->load()   : 0.7f;
    padEngine.renderInto (buffer, padPitchBendSemis, padCutoff, padReso);

    // --- Efeitos master globais (sempre audiveis): Drive (saturacao) + Reverb ---
    const float drive = driveParam != nullptr ? driveParam->load() : 0.0f;
    if (drive > 0.001f)
    {
        const float k = 1.0f + drive * 20.0f;          // intensidade da saturacao
        const float norm = 1.0f / std::tanh (k);        // makeup p/ manter o nivel
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                data[n] = std::tanh (data[n] * k) * norm;
        }
    }

    const float reverbMix = reverbParam != nullptr ? reverbParam->load() : 0.0f;
    if (reverbMix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.6f;
        rp.damping  = 0.4f;
        rp.width    = 1.0f;
        rp.wetLevel = reverbMix;
        rp.dryLevel = 1.0f - reverbMix * 0.5f; // mantem o som seco presente
        reverb.setParameters (rp);

        if (buffer.getNumChannels() >= 2)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
        else if (buffer.getNumChannels() == 1)
            reverb.processMono (buffer.getWritePointer (0), buffer.getNumSamples());
    }

    // Mixer master: ganho global + guarda contra clipping (hard clip de seguranca).
    const float gain = gainParam != nullptr ? gainParam->load() : 0.7f;
    buffer.applyGain (gain);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            data[n] = juce::jlimit (-1.0f, 1.0f, data[n]);
    }
}

void PianoSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PianoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            // Macro mappings use fixed defaults (set in the constructor); not loaded
            // from state so they survive across sessions without being clobbered.
        }
    }
}

juce::AudioProcessorEditor* PianoSynthAudioProcessor::createEditor()
{
    return new PianoSynthAudioProcessorEditor (*this);
}

// Ponto de entrada exigido pelo wrapper (VST3/AU/Standalone).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PianoSynthAudioProcessor();
}
