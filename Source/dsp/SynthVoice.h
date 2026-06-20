#pragma once

#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include "Oscillator.h"

/**
    Uma voz de polifonia. O juce::Synthesiser instancia N copias destas e faz
    a alocacao/roubo de vozes. Cada voz roda 100% na thread de audio:
    nenhuma alocacao, lock ou chamada bloqueante em renderNextBlock().

    Cadeia de sinal por voz:
        osc1 + osc2(detune)  ->  mix  ->  filtro low-pass ressonante  ->  ADSR  ->  ganho

    Todos os parametros chegam da GUI via ponteiros atomicos do APVTS
    (leitura lock-free, sem glitch de audio).
*/
class SynthVoice : public juce::SynthesiserVoice
{
public:
    struct ParameterPointers
    {
        std::atomic<float>* waveform  = nullptr; // indice (0=Seno,1=Serra,2=Quadrada,3=Triangulo)
        std::atomic<float>* attack    = nullptr; // segundos
        std::atomic<float>* decay     = nullptr; // segundos
        std::atomic<float>* sustain   = nullptr; // 0..1
        std::atomic<float>* release   = nullptr; // segundos
        std::atomic<float>* detune    = nullptr; // cents do osc2 (-50..+50)
        std::atomic<float>* oscMix    = nullptr; // 0=so osc1 ... 1=so osc2
        std::atomic<float>* cutoff    = nullptr; // Hz do filtro
        std::atomic<float>* resonance = nullptr; // Q do filtro
        std::atomic<float>* modWheel  = nullptr; // 0..1 (mod wheel -> profundidade de vibrato)
    };

    explicit SynthVoice (ParameterPointers paramPtrs) : params (paramPtrs) {}

    bool canPlaySound (juce::SynthesiserSound* sound) override;

    void setCurrentPlaybackSampleRate (double newRate) override;

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;

    void stopNote (float velocity, bool allowTailOff) override;

    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int controllerNumber, int newControllerValue) override;

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;

private:
    void updateAdsrParameters();
    void setPitchBendFromWheel (int wheelValue) noexcept; // 0..16383, centro 8192
    Oscillator::Waveform currentWaveform() const noexcept;

    ParameterPointers params;
    Oscillator        osc1, osc2;
    juce::ADSR        adsr;

    juce::dsp::StateVariableTPTFilter<float> filter; // low-pass ressonante, por amostra

    double baseFrequencyHz   = 440.0;
    float  velocityGain      = 1.0f;
    float  pitchBendSemitones = 0.0f; // deslocamento do pitch wheel (+/- 2 semitons)
    double lfoPhase           = 0.0;   // LFO de vibrato [0,1)

    static constexpr float kPitchBendRange  = 2.0f;  // semitons no fim de curso do wheel
    static constexpr float kVibratoRange    = 0.5f;  // semitons no mod wheel maximo
    static constexpr float kVibratoRateHz   = 5.5f;  // velocidade do vibrato
};
