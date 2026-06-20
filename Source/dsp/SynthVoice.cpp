#include "SynthVoice.h"
#include "SynthSound.h"

#include <cmath>

bool SynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*> (sound) != nullptr;
}

void SynthVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

    if (newRate > 0.0)
    {
        osc1.setSampleRate (newRate);
        osc2.setSampleRate (newRate);
        adsr.setSampleRate (newRate);

        // Filtro processado amostra-a-amostra (mono): block size nao importa aqui.
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = newRate;
        spec.maximumBlockSize = 512;
        spec.numChannels      = 1;
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }
}

Oscillator::Waveform SynthVoice::currentWaveform() const noexcept
{
    const int idx = params.waveform != nullptr
                      ? juce::jlimit (0, 3, static_cast<int> (params.waveform->load()))
                      : 0;
    return static_cast<Oscillator::Waveform> (idx);
}

void SynthVoice::setPitchBendFromWheel (int wheelValue) noexcept
{
    // wheelValue: 0..16383, centro 8192. Mapeia p/ +/- kPitchBendRange semitons.
    pitchBendSemitones = (static_cast<float> (wheelValue) - 8192.0f) / 8192.0f * kPitchBendRange;
}

void SynthVoice::startNote (int midiNoteNumber, float velocity,
                            juce::SynthesiserSound* /*sound*/, int currentPitchWheelPosition)
{
    baseFrequencyHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    setPitchBendFromWheel (currentPitchWheelPosition);
    lfoPhase = 0.0;

    osc1.setFrequency (baseFrequencyHz);
    osc1.reset();
    osc2.reset();
    filter.reset();

    // Velocity -> ganho com PISO: notas fracas (ou clique no topo da tecla) seguem
    // audiveis. Antes era velocity^2, que silenciava velocities baixas.
    velocityGain = 0.35f + 0.65f * velocity;

    updateAdsrParameters();
    adsr.noteOn();
}

void SynthVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff();            // entra no estagio de Release; a voz se libera sozinha
    }
    else
    {
        adsr.reset();              // corte imediato (ex.: roubo de voz)
        clearCurrentNote();
    }
}

void SynthVoice::pitchWheelMoved (int newPitchWheelValue)
{
    setPitchBendFromWheel (newPitchWheelValue);
}

void SynthVoice::controllerMoved (int /*controllerNumber*/, int /*newControllerValue*/) {}

void SynthVoice::updateAdsrParameters()
{
    juce::ADSR::Parameters p;
    p.attack  = params.attack  != nullptr ? params.attack->load()  : 0.01f;
    p.decay   = params.decay   != nullptr ? params.decay->load()   : 0.10f;
    p.sustain = params.sustain != nullptr ? params.sustain->load() : 0.80f;
    p.release = params.release != nullptr ? params.release->load() : 0.20f;
    adsr.setParameters (p);
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                  int startSample, int numSamples)
{
    if (! adsr.isActive())
        return;

    updateAdsrParameters();

    // Parametros podem mudar em tempo real pela GUI (leitura atomica lock-free).
    const auto wave = currentWaveform();
    osc1.setWaveform (wave);
    osc2.setWaveform (wave);

    const float detuneCents = params.detune != nullptr ? params.detune->load() : 0.0f;
    const double detuneRatio = std::pow (2.0, detuneCents / 1200.0);

    // Pitch: bend (wheel) + vibrato (mod wheel via LFO senoidal).
    const double bendMult = std::pow (2.0, pitchBendSemitones / 12.0);
    const float  modDepth = params.modWheel != nullptr ? juce::jlimit (0.0f, 1.0f, params.modWheel->load()) : 0.0f;
    const bool   vibratoOn = modDepth > 0.001f;
    const double lfoInc = static_cast<double> (kVibratoRateHz) / getSampleRate();

    // Sem vibrato: define a frequencia uma vez (caminho barato).
    if (! vibratoOn)
    {
        osc1.setFrequency (baseFrequencyHz * bendMult);
        osc2.setFrequency (baseFrequencyHz * bendMult * detuneRatio);
    }

    const float mix = params.oscMix != nullptr ? juce::jlimit (0.0f, 1.0f, params.oscMix->load()) : 0.0f;

    const double nyquistGuard = 0.49 * getSampleRate();
    const float cutoff = params.cutoff != nullptr
                           ? juce::jlimit (20.0f, static_cast<float> (nyquistGuard), params.cutoff->load())
                           : static_cast<float> (nyquistGuard);
    const float reso = params.resonance != nullptr ? juce::jlimit (0.1f, 8.0f, params.resonance->load()) : 0.7f;
    filter.setCutoffFrequency (cutoff);
    filter.setResonance (reso);

    const int numChannels = outputBuffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (vibratoOn) // recalcula a frequencia por amostra para um vibrato suave
        {
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;

            const double vibSemis = modDepth * kVibratoRange
                                  * std::sin (juce::MathConstants<double>::twoPi * lfoPhase);
            const double mult = bendMult * std::pow (2.0, vibSemis / 12.0);
            osc1.setFrequency (baseFrequencyHz * mult);
            osc2.setFrequency (baseFrequencyHz * mult * detuneRatio);
        }

        const float env = adsr.getNextSample();                  // 0..1
        const float oscSum = osc1.getNextSample() * (1.0f - mix)
                           + osc2.getNextSample() * mix;          // -1..1
        const float filtered = filter.processSample (0, oscSum);
        const float value = filtered * env * velocityGain;

        for (int ch = 0; ch < numChannels; ++ch)
            outputBuffer.addSample (ch, startSample + sample, value);

        // ADSR concluiu o Release => libera a voz para realocacao.
        if (! adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
