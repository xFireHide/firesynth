#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/**
    Som do sintetizador. Em juce::Synthesiser, um "Sound" descreve a quais
    notas/canais uma voz se aplica. Aqui aceitamos toda a extensao do teclado
    em todos os canais MIDI — uma unica timbragem para o instrumento inteiro.
*/
class SynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote    (int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel (int /*midiChannel*/)    override { return true; }
};
