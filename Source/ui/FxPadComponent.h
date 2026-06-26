#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SynthLookAndFeel.h"

/**
    Chip arrastavel da paleta de efeitos. Inicia um drag&drop INTERNO (dentro do
    editor, que e um juce::DragAndDropContainer) carregando o indice do efeito.
*/
class FxChip : public juce::Component
{
public:
    int          fxIndex = -1;
    juce::String label;

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! container->isDragAndDropActive())
                container->startDragging (juce::var (fxIndex), this);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (juce::Colour (SynthLookAndFeel::accent2).withAlpha (0.9f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
        g.drawText (label, r, juce::Justification::centred);
    }
};
