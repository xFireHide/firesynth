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

/**
    Pad de efeito de voz: alvo de drag&drop dos chips (atribui o efeito) e
    toggle (clique liga o efeito na voz; clicar de novo desliga).
*/
class FxPadComponent : public juce::Component,
                       public juce::DragAndDropTarget
{
public:
    std::function<void (int)>          onAssign;      // efeito atribuido (indice)
    std::function<void (int, bool)>    onHold;        // segura/solta (indice, ativo)
    std::function<juce::String (int)>  nameOf;        // indice -> nome exibido

    void setIndex (int i) { index = i; repaint(); }

    int  getAssigned() const noexcept { return assigned; }
    void clearHeld()
    {
        if (held) { held = false; if (onHold && assigned >= 0) onHold (assigned, false); repaint(); }
    }

    // --- Drag&drop interno (recebe um chip) ---
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.isInt(); }
    void itemDragEnter (const SourceDetails&) override { over = true;  repaint(); }
    void itemDragExit  (const SourceDetails&) override { over = false; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        over = false;
        clearHeld();                         // troca de efeito nao deixa o anterior preso
        assigned = (int) d.description;
        if (onAssign) onAssign (assigned);
        repaint();
    }

    // --- Toggle: clique liga, clique desliga ---
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (assigned < 0) return;
        // So alterna em clique real (sem drag) e com o ponteiro ainda sobre o pad.
        if (e.mouseWasDraggedSinceMouseDown()) return;
        if (! getLocalBounds().contains (e.getPosition())) return;

        held = ! held;
        if (onHold) onHold (assigned, held);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const bool loaded = assigned >= 0;

        auto base = juce::Colour (loaded ? SynthLookAndFeel::accent2 : SynthLookAndFeel::panel)
                        .withMultipliedBrightness (loaded ? 0.85f : 1.0f);
        if (held)
            base = juce::Colour (SynthLookAndFeel::accent);
        g.setColour (base);
        g.fillRoundedRectangle (r, 6.0f);

        g.setColour (over ? juce::Colour (SynthLookAndFeel::accent)
                          : juce::Colour (0xff3a3f47));
        g.drawRoundedRectangle (r, 6.0f, over ? 2.5f : 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (10.0f);
        g.drawText ("FX " + juce::String (index), r.reduced (5.0f).removeFromTop (13.0f),
                    juce::Justification::topLeft);

        g.setColour (juce::Colours::white.withAlpha (loaded ? 0.95f : 0.4f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
        g.drawFittedText (loaded && nameOf ? nameOf (assigned) : "solte FX",
                          r.reduced (5.0f).withTrimmedTop (12.0f).toNearestInt(),
                          juce::Justification::centred, 2);
    }

private:
    int  index    = 1;
    int  assigned = -1;
    bool over     = false;
    bool held     = false;
};
