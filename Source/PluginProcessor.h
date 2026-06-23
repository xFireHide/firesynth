#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "dsp/PadEngine.h"
#include "dsp/VoiceFX.h"

/**
    Coracao do plugin/standalone.

    Responsabilidades:
      - hospedar o juce::Synthesiser (gerenciador de polifonia / voice stealing);
      - expor os parametros via APVTS (ponte lock-free GUI <-> audio);
      - rotear o MIDI de entrada e renderizar o buffer master (mix + ganho global).

    A thread de audio so toca processBlock(); a GUI so escreve no APVTS. As duas
    nunca compartilham estado mutavel diretamente.
*/
class PianoSynthAudioProcessor : public juce::AudioProcessor
{
public:
    PianoSynthAudioProcessor();
    ~PianoSynthAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PianoSynth"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Estado do teclado on-screen (Standalone). Thread-safe por design no JUCE:
    // a GUI empurra eventos e o audio os consome em processBlock.
    juce::MidiKeyboardState keyboardState;

    // Telemetria de MIDI para a GUI (escrita no audio, lida na GUI — lock-free).
    std::atomic<int> midiNoteOnCount { 0 };  // incrementa a cada Note On recebido
    std::atomic<int> lastNoteNumber  { -1 };  // ultima nota tocada (0..127)
    // Modulation wheel (CC 1) -> profundidade de vibrato (0..1). Lido pelas vozes.
    std::atomic<float> modWheelValue { 0.0f };

    // Volume MIDI (CC 7) -> Master Gain. Audio thread enfileira; GUI aplica no parametro.
    std::atomic<float> pendingMidiVolume { -1.0f };

    // "Knob universal": os 4 knobs do AMW Mini 32 mandam Pitch Bend identico (provado),
    // entao sao indistinguiveis. Solucao: controlam o knob da tela em FOCO, com soft
    // takeover. A GUI le este valor e aplica no parametro focado.
    std::atomic<float> knobValue01 { -1.0f }; // ultimo Pitch Bend normalizado (-1 = nada novo)

    // Reproducao de amostras dos 8 pads (drag&drop na GUI; gatilho por clique ou MIDI).
    PadEngine padEngine;

    // Efeitos aplicados a voz ao vivo (microfone). A GUI liga/desliga cada efeito
    // segurando um pad de FX; a intensidade vem de parametros do APVTS (knob-mapeavel).
    VoiceFX voiceFx;
    // Pads do AMW Mini 32: canal 10, notas 30..37 (8 pads).
    static constexpr int kPadMidiChannel = 10;
    static constexpr int kPadBaseNote    = 30;

    // Assinatura de um controle continuo (CC, Pitch Bend ou Aftertouch) + canal.
    //   bits 24-31: tipo (1=CC, 2=PitchBend, 3=Aftertouch)
    //   bits  8-15: canal MIDI (1..16)
    //   bits  0-7 : numero do CC (0 p/ pitch bend / aftertouch)
    // Retorna false se a mensagem nao for um controle continuo "aprendivel".
    static bool classifyControl (const juce::MidiMessage& m, int& signature, float& normalised);

    // --- Macros de hardware: mapeiam knobs/controles -> parametros, via MIDI Learn ---
    //  - Audio thread: so escreve 'pending' (RT-safe, sem lock/alloc).
    //  - Message thread (GUI): consome 'pending' e aplica no parametro do APVTS.
    struct MidiMacro
    {
        juce::String       targetParamID;       // controlled parameter (APVTS id); set in ctor/GUI
        std::atomic<int>   learnedSig { -1 };     // CC signature this knob sends (-1 = none)
        std::atomic<float> pending { -1.0f };     // normalised value to apply (-1 = nothing)
    };

    static constexpr int kNumMacros = 4;
    std::array<MidiMacro, kNumMacros> macros;

    static constexpr int kNumVoices = 16; // limite de polifonia

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::Synthesiser synth;
    std::atomic<float>* gainParam   = nullptr; // ganho master (linear, 0..1)
    std::atomic<float>* cutoffParam = nullptr; // p/ aplicar o filtro tambem nos pads
    std::atomic<float>* resoParam   = nullptr;
    std::atomic<float>* driveParam  = nullptr; // saturacao master (0..1)
    std::atomic<float>* reverbParam = nullptr; // mix de reverb master (0..1)
    float padPitchBendSemis = 0.0f;            // pitch bend atual aplicado aos pads (audio thread)

    juce::Reverb reverb; // efeito master global

    std::atomic<float>* voiceMonitorParam = nullptr; // retorno da voz on/off (0/1)
    std::atomic<float>* voiceLevelParam   = nullptr; // volume do retorno da voz (0..1)

    // Buffer reutilizado p/ a voz capturada do microfone (sem alocacao no audio thread).
    juce::AudioBuffer<float> voiceBuffer;
    // Ponteiros atomicos (APVTS) de intensidade de cada efeito de voz.
    std::array<std::atomic<float>*, VoiceFX::kNumFx> fxAmountParam {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoSynthAudioProcessor)
};
