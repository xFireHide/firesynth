#pragma once

#include <array>
#include <atomic>
#include <juce_dsp/juce_dsp.h>

/**
    Cadeia de efeitos aplicada a voz ao vivo (entrada de microfone).

    Modelo de uso:
      - cada efeito e ligado/desligado segurando um pad de FX na GUI (momentaneo);
      - a intensidade de cada efeito e um parametro do APVTS (0..1), entao um knob
        de hardware pode controla-la via o mesmo sistema de MIDI Learn do synth.

    Thread-safety:
      - 'active[]' e escrito pela thread de mensagens (mouse) e lido pelo audio (atomic);
      - 'amount[]' aponta para floats atomicos do APVTS (escrita GUI, leitura audio);
      - process() NUNCA aloca/trava: todo DSP e pre-alocado em prepare().
*/
class VoiceFX
{
public:
    // Ordem tambem define a cadeia de processamento (drive -> ... -> reverb).
    enum Type { drive = 0, reverb, delay, chorus, crush, phone, robot, kNumFx };

    VoiceFX() = default; // o macro NON_COPYABLE suprime o default implicito; reintroduz.

    /** Id do parametro APVTS de intensidade do efeito (ex.: "fxDriveAmt"). */
    static const char* paramId (int fx) noexcept;
    /** Nome curto exibido no chip/pad (ex.: "Drive"). */
    static const char* displayName (int fx) noexcept;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    void setActive (int fx, bool on) noexcept
    {
        if (juce::isPositiveAndBelow (fx, (int) kNumFx))
            active[(size_t) fx].store (on, std::memory_order_relaxed);
    }

    bool isActive (int fx) const noexcept
    {
        return juce::isPositiveAndBelow (fx, (int) kNumFx)
            && active[(size_t) fx].load (std::memory_order_relaxed);
    }

    bool anyActive() const noexcept
    {
        for (auto& a : active)
            if (a.load (std::memory_order_relaxed))
                return true;
        return false;
    }

    /** Aponta o efeito para o float atomico (APVTS) que dosa sua intensidade. */
    void setAmountParam (int fx, std::atomic<float>* p) noexcept
    {
        if (juce::isPositiveAndBelow (fx, (int) kNumFx))
            amount[(size_t) fx] = p;
    }

    /** Aplica, em cadeia fixa, somente os efeitos ativos. Thread de audio. */
    void process (juce::AudioBuffer<float>& voice, int numSamples) noexcept;

private:
    float amountOf (int fx) const noexcept
    {
        auto* p = amount[(size_t) fx];
        return p != nullptr ? juce::jlimit (0.0f, 1.0f, p->load (std::memory_order_relaxed)) : 0.5f;
    }

    std::array<std::atomic<bool>, kNumFx>   active {};
    std::array<std::atomic<float>*, kNumFx> amount {};

    double sampleRate = 44100.0;
    int    numCh      = 2;

    juce::Reverb reverbDsp;                                 // reverb da voz (independente do master)
    juce::dsp::DelayLine<float> delayLine { 1 << 17 };      // ate ~2.7 s @ 48 kHz
    juce::dsp::Chorus<float> chorusDsp;
    juce::dsp::StateVariableTPTFilter<float> phoneHP, phoneLP; // "telefone" = band-pass

    double               robotPhase = 0.0;                 // fase do ring-mod
    std::array<float, 2> crushHold  { { 0.0f, 0.0f } };    // ultimo valor segurado (bitcrush)
    std::array<float, 2> crushPhase { { 0.0f, 0.0f } };    // acumulador de downsample

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceFX)
};
