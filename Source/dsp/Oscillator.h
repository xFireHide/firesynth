#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

/**
    Oscilador monofonico, amostra-a-amostra, real-time safe (sem alocacao/locks).

    Formas de onda: Seno (puro), Serra e Quadrada. Serra/Quadrada usam PolyBLEP
    para suavizar as descontinuidades e reduzir o aliasing — o "naive saw/square"
    geraria parciais acima de Nyquist que rebatem como ruido nas notas agudas.
*/
class Oscillator
{
public:
    enum class Waveform { Sine = 0, Saw, Square, Triangle };

    void setSampleRate (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        updateIncrement();
    }

    void setFrequency (double newFrequencyHz) noexcept
    {
        frequencyHz = newFrequencyHz;
        updateIncrement();
    }

    void setWaveform (Waveform w) noexcept { waveform = w; }

    /** Zera a fase. Chamar no inicio de cada nota para evitar cliques de fase. */
    void reset() noexcept { phase = 0.0; }

    /** Proxima amostra no intervalo [-1, 1]. Real-time safe. */
    float getNextSample() noexcept
    {
        float value = 0.0f;

        switch (waveform)
        {
            case Waveform::Sine:     value = static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * phase)); break;
            case Waveform::Saw:      value = renderSaw();    break;
            case Waveform::Square:   value = renderSquare(); break;
            case Waveform::Triangle: value = static_cast<float> (2.0 * std::abs (2.0 * phase - 1.0) - 1.0); break;
        }

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;

        return value;
    }

private:
    void updateIncrement() noexcept
    {
        increment = (sampleRate > 0.0) ? (frequencyHz / sampleRate) : 0.0;
    }

    // PolyBLEP: termo de correcao em torno de cada descontinuidade (t e dt em ciclos).
    static double polyBlep (double t, double dt) noexcept
    {
        if (t < dt)                 // borda de subida
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)           // borda de descida
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    float renderSaw() noexcept
    {
        double v = 2.0 * phase - 1.0;          // rampa "naive"
        v -= polyBlep (phase, increment);
        return static_cast<float> (v);
    }

    float renderSquare() noexcept
    {
        double v = (phase < 0.5) ? 1.0 : -1.0; // pulso 50% "naive"
        v += polyBlep (phase, increment);
        double t2 = phase + 0.5;
        if (t2 >= 1.0) t2 -= 1.0;
        v -= polyBlep (t2, increment);
        return static_cast<float> (v);
    }

    double sampleRate  = 44100.0;
    double frequencyHz = 440.0;
    double phase       = 0.0;   // fase normalizada [0, 1)
    double increment   = 0.0;   // ciclos por amostra (dt)
    Waveform waveform  = Waveform::Sine;
};
