#include "PadEngine.h"

bool PadEngine::loadFile (int pad, const juce::File& file, juce::String& errorMessage)
{
    if (! juce::isPositiveAndBelow (pad, kNumPads))
    {
        errorMessage = "Pad invalido";
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        errorMessage = "Formato nao suportado: " + file.getFileName();
        return false;
    }

    const int totalLen = static_cast<int> (reader->lengthInSamples);
    if (totalLen <= 0)
    {
        errorMessage = "Arquivo vazio: " + file.getFileName();
        return false;
    }

    const int maxSamples = static_cast<int> (reader->sampleRate * kMaxSeconds);
    const int numSamples = juce::jmin (totalLen, maxSamples);
    const int numChannels = juce::jmax (1, static_cast<int> (reader->numChannels));

    PadSample::Ptr loaded = new PadSample();
    loaded->audio.setSize (numChannels, numSamples);
    reader->read (&loaded->audio, 0, numSamples, 0, true, true);
    loaded->sourceSampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;

    keepAlive.add (loaded);                                              // mantem vivo na sessao
    pads[(size_t) pad].sample.store (loaded.get(), std::memory_order_release); // audio passa a ver
    names[(size_t) pad] = file.getFileName();
    return true;
}

void PadEngine::renderInto (juce::AudioBuffer<float>& output,
                            float pitchSemitones, float cutoffHz, float resonance) noexcept
{
    const int numOut = output.getNumSamples();
    const int outCh  = output.getNumChannels();

    // Mix dos pads num buffer separado (para nao re-filtrar o synth, ja filtrado).
    if (scratch.getNumChannels() < outCh || scratch.getNumSamples() < numOut)
        return; // seguranca: scratch deve ter sido dimensionado em prepare()
    scratch.clear (0, numOut);

    // Pitch bend (mesmo do pitch wheel) acelera/desacelera a reproducao.
    const double pitchMult = std::pow (2.0, pitchSemitones / 12.0);

    for (auto& pad : pads)
    {
        if (pad.triggerFlag.exchange (false, std::memory_order_relaxed))
        {
            pad.readPos = 0.0;
            pad.playing = true;
            pad.activity.store (true, std::memory_order_relaxed);
        }

        if (! pad.playing)
            continue;

        auto* s = pad.sample.load (std::memory_order_acquire);
        if (s == nullptr)
        {
            pad.playing = false;
            continue;
        }

        const int    srcLen = s->audio.getNumSamples();
        const int    srcCh  = s->audio.getNumChannels();
        const double ratio  = (s->sourceSampleRate / deviceSampleRate) * pitchMult;

        for (int i = 0; i < numOut && pad.playing; ++i)
        {
            const int pos = static_cast<int> (pad.readPos);
            if (pos + 1 >= srcLen)
            {
                pad.playing = false;
                break;
            }

            const float frac = static_cast<float> (pad.readPos - pos);
            for (int ch = 0; ch < outCh; ++ch)
            {
                const float* src = s->audio.getReadPointer (juce::jmin (ch, srcCh - 1));
                const float value = src[pos] + frac * (src[pos + 1] - src[pos]);
                scratch.addSample (ch, i, value);
            }

            pad.readPos += ratio;
        }
    }

    // Filtro (Cutoff/Resonance) aplicado ao mix dos pads.
    const double nyquistGuard = 0.49 * deviceSampleRate;
    filter.setCutoffFrequency (juce::jlimit (20.0f, static_cast<float> (nyquistGuard), cutoffHz));
    filter.setResonance (juce::jlimit (0.1f, 8.0f, resonance));

    juce::dsp::AudioBlock<float> block (scratch);
    auto sub = block.getSubBlock (0, (size_t) numOut).getSubsetChannelBlock (0, (size_t) outCh);
    juce::dsp::ProcessContextReplacing<float> ctx (sub);
    filter.process (ctx);

    // Soma o resultado filtrado na saida.
    for (int ch = 0; ch < outCh; ++ch)
        output.addFrom (ch, 0, scratch, ch, 0, numOut);
}
