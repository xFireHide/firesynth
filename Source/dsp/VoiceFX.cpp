#include "VoiceFX.h"

#include <cmath>

const char* VoiceFX::paramId (int fx) noexcept
{
    switch (fx)
    {
        case drive:  return "fxDriveAmt";
        case reverb: return "fxReverbAmt";
        case delay:  return "fxDelayAmt";
        case chorus: return "fxChorusAmt";
        case crush:  return "fxCrushAmt";
        case phone:  return "fxPhoneAmt";
        case robot:  return "fxRobotAmt";
        default:     return "";
    }
}

const char* VoiceFX::displayName (int fx) noexcept
{
    switch (fx)
    {
        case drive:  return "Drive";
        case reverb: return "Reverb";
        case delay:  return "Delay";
        case chorus: return "Chorus";
        case crush:  return "Crush";
        case phone:  return "Phone";
        case robot:  return "Robot";
        default:     return "?";
    }
}

void VoiceFX::prepare (double sr, int maxBlockSize, int numChannels)
{
    sampleRate = sr;
    numCh      = juce::jlimit (1, (int) crushHold.size(), numChannels);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = (juce::uint32) juce::jmax (1, maxBlockSize);
    spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

    reverbDsp.setSampleRate (sr);
    reverbDsp.reset();

    delayLine.prepare (spec);
    delayLine.reset();

    chorusDsp.prepare (spec);
    chorusDsp.reset();

    phoneHP.prepare (spec);
    phoneHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    phoneHP.setCutoffFrequency (300.0f);

    phoneLP.prepare (spec);
    phoneLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    phoneLP.setCutoffFrequency (3200.0f);

    robotPhase = 0.0;
    crushHold.fill (0.0f);
    crushPhase.fill (0.0f);
}

void VoiceFX::process (juce::AudioBuffer<float>& voice, int numSamples) noexcept
{
    const int chs = juce::jmin (voice.getNumChannels(), numCh);
    if (chs <= 0 || numSamples <= 0)
        return;

    // --- Drive: saturacao tanh com makeup (mesma curva do drive master) ---
    if (isActive (drive))
    {
        const float amt  = amountOf (drive);
        const float k    = 1.0f + amt * 20.0f;
        const float norm = 1.0f / std::tanh (k);
        for (int ch = 0; ch < chs; ++ch)
        {
            auto* d = voice.getWritePointer (ch);
            for (int n = 0; n < numSamples; ++n)
                d[n] = std::tanh (d[n] * k) * norm;
        }
    }

    // --- Crush: reducao de bits + downsample (sample & hold) ---
    if (isActive (crush))
    {
        const float amt    = amountOf (crush);
        const int   bits   = juce::jmax (1, (int) juce::jmap (amt, 0.0f, 1.0f, 16.0f, 3.0f));
        const float levels = (float) (1 << bits);
        const float step   = 1.0f + amt * 19.0f; // 1x..20x downsample
        for (int ch = 0; ch < chs; ++ch)
        {
            auto* d   = voice.getWritePointer (ch);
            float hold = crushHold[(size_t) ch];
            float ph   = crushPhase[(size_t) ch];
            for (int n = 0; n < numSamples; ++n)
            {
                ph += 1.0f;
                if (ph >= step)
                {
                    ph  -= step;
                    hold = std::round (d[n] * levels) / levels;
                }
                d[n] = hold;
            }
            crushHold[(size_t) ch]  = hold;
            crushPhase[(size_t) ch] = ph;
        }
    }

    // --- Phone: band-pass (telefone) + leve grit proporcional a intensidade ---
    if (isActive (phone))
    {
        juce::dsp::AudioBlock<float> block (voice);
        auto sub = block.getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) chs);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        phoneHP.process (ctx);
        phoneLP.process (ctx);

        const float amt  = amountOf (phone);
        const float k    = 1.0f + amt * 8.0f;
        const float norm = 1.0f / std::tanh (k);
        for (int ch = 0; ch < chs; ++ch)
        {
            auto* d = voice.getWritePointer (ch);
            for (int n = 0; n < numSamples; ++n)
                d[n] = std::tanh (d[n] * k) * norm;
        }
    }

    // --- Chorus ---
    if (isActive (chorus))
    {
        const float amt = amountOf (chorus);
        chorusDsp.setRate (0.4f + amt * 4.0f);
        chorusDsp.setDepth (0.2f + amt * 0.6f);
        chorusDsp.setCentreDelay (7.0f);
        chorusDsp.setFeedback (amt * 0.4f);
        chorusDsp.setMix (0.5f);

        juce::dsp::AudioBlock<float> block (voice);
        auto sub = block.getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) chs);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        chorusDsp.process (ctx);
    }

    // --- Delay com realimentacao ---
    if (isActive (delay))
    {
        const float amt    = amountOf (delay);
        const float maxDly = (float) (delayLine.getMaximumDelayInSamples() - 1);
        const float dlyS   = juce::jmin (maxDly, (float) (sampleRate * (0.12f + amt * 0.40f)));
        const float fb     = 0.25f + amt * 0.50f;
        const float mix    = 0.5f;
        delayLine.setDelay (dlyS);
        for (int ch = 0; ch < chs; ++ch)
        {
            auto* d = voice.getWritePointer (ch);
            for (int n = 0; n < numSamples; ++n)
            {
                const float in  = d[n];
                const float dly = delayLine.popSample (ch);
                delayLine.pushSample (ch, in + dly * fb);
                d[n] = in * (1.0f - mix) + dly * mix;
            }
        }
    }

    // --- Robot: ring modulation (mesmo modulador para todos os canais) ---
    if (isActive (robot))
    {
        const float  amt  = amountOf (robot);
        const float  freq = 40.0f + amt * 160.0f;                 // 40..200 Hz
        const float  mix  = 0.6f + amt * 0.4f;
        const double inc  = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
        double ph = robotPhase;
        for (int n = 0; n < numSamples; ++n)
        {
            const float m = (float) std::sin (ph);
            ph += inc;
            if (ph >= juce::MathConstants<double>::twoPi)
                ph -= juce::MathConstants<double>::twoPi;
            for (int ch = 0; ch < chs; ++ch)
            {
                auto* d = voice.getWritePointer (ch);
                d[n] = d[n] * (1.0f - mix) + (d[n] * m) * mix;
            }
        }
        robotPhase = ph;
    }

    // --- Reverb (por ultimo, como cauda) ---
    if (isActive (reverb))
    {
        const float amt = amountOf (reverb);
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.5f + amt * 0.45f;
        rp.damping  = 0.4f;
        rp.width    = 1.0f;
        rp.wetLevel = 0.3f + amt * 0.6f;
        rp.dryLevel = 0.7f;
        reverbDsp.setParameters (rp);

        if (chs >= 2)
            reverbDsp.processStereo (voice.getWritePointer (0), voice.getWritePointer (1), numSamples);
        else
            reverbDsp.processMono (voice.getWritePointer (0), numSamples);
    }
}
