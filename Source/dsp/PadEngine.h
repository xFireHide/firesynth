#pragma once

#include <array>
#include <atomic>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

/**
    Motor de reproducao de amostras para os 8 pads.

    Thread-safety (o ponto critico):
      - Carregar arquivo: thread de mensagens (drag&drop). Aloca o AudioBuffer e
        publica um ponteiro atomico para a thread de audio.
      - Reproduzir/misturar: thread de audio. So le ponteiro atomico + estado proprio;
        nunca aloca, trava ou abre arquivo.
      - As amostras carregadas ficam vivas em 'keepAlive' (lista da thread de mensagens)
        durante toda a sessao, entao o ponteiro cru lido pelo audio nunca fica pendurado.

    Playback com interpolacao linear e razao sourceRate/deviceRate — toca em qualquer
    sample rate, mono ou estereo.
*/
class PadEngine
{
public:
    static constexpr int kNumPads = 16; // banks A (1-8) + B (9-16) via the A/B button

    PadEngine() { formatManager.registerBasicFormats(); }

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        deviceSampleRate = sampleRate;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = (juce::uint32) juce::jmax (1, maxBlockSize);
        spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

        scratch.setSize (juce::jmax (1, numChannels), juce::jmax (1, maxBlockSize));
    }

    /** Carrega um arquivo de audio no pad (thread de mensagens). */
    bool loadFile (int pad, const juce::File& file, juce::String& errorMessage);

    /** Dispara a reproducao do pad. Seguro de qualquer thread. */
    void trigger (int pad) noexcept
    {
        if (juce::isPositiveAndBelow (pad, kNumPads))
            pads[(size_t) pad].triggerFlag.store (true, std::memory_order_relaxed);
    }

    /** Mistura os pads tocando no buffer de saida, aplicando pitch bend + filtro
        (mesmos efeitos do synth). Thread de audio. */
    void renderInto (juce::AudioBuffer<float>& output,
                     float pitchSemitones, float cutoffHz, float resonance) noexcept;

    /** Nome do arquivo carregado, ou vazio (thread de mensagens). */
    juce::String getName (int pad) const
    {
        return juce::isPositiveAndBelow (pad, kNumPads) ? names[(size_t) pad] : juce::String();
    }

    /** True (uma vez) se o pad disparou desde a ultima chamada — para acender na GUI. */
    bool consumeActivity (int pad) noexcept
    {
        return juce::isPositiveAndBelow (pad, kNumPads)
                 && pads[(size_t) pad].activity.exchange (false, std::memory_order_relaxed);
    }

private:
    struct PadSample : public juce::ReferenceCountedObject
    {
        using Ptr = juce::ReferenceCountedObjectPtr<PadSample>;
        juce::AudioBuffer<float> audio;
        double sourceSampleRate = 44100.0;
    };

    struct Pad
    {
        std::atomic<PadSample*> sample      { nullptr }; // lido pelo audio (cru)
        std::atomic<bool>       triggerFlag { false };
        std::atomic<bool>       activity    { false };
        double readPos = 0.0;   // estado do audio
        bool   playing = false; // estado do audio
    };

    std::array<Pad, kNumPads>        pads;
    juce::AudioFormatManager         formatManager;
    juce::ReferenceCountedArray<PadSample> keepAlive; // mantem amostras vivas (msg thread)
    std::array<juce::String, kNumPads> names;
    double deviceSampleRate = 44100.0;

    juce::dsp::StateVariableTPTFilter<float> filter; // efeito de filtro nos pads
    juce::AudioBuffer<float> scratch;                // mix dos pads antes do filtro

    static constexpr double kMaxSeconds = 30.0; // limite por amostra

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadEngine)
};
