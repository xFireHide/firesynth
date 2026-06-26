#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SynthLookAndFeel.h"

/**
    Um pad on-screen com dois modos, decididos pelo que voce solta nele:

      - SAMPLE: solte um arquivo de audio -> clique (ou pad de hardware) toca o sample;
      - FX DE VOZ: solte um chip de efeito -> clique liga/desliga o efeito no microfone.

    Soltar um audio num pad de FX (ou vice-versa) troca o modo do pad — os dois sao
    mutuamente exclusivos. Aceita tanto drag&drop de arquivos (sample) quanto o
    drag&drop interno dos chips (FX, descricao = indice inteiro do efeito).
*/
class PadComponent : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     public juce::DragAndDropTarget
{
public:
    // --- Sample ---
    std::function<void()>                   onTrigger;     // clique no pad (modo sample)
    std::function<void (const juce::File&)> onFileDropped; // arquivo solto no pad

    // --- FX de voz ---
    std::function<void (int)>          onFxAssign;  // efeito atribuido (indice)
    std::function<void (int, bool)>    onFxHold;    // liga/desliga (indice, ativo)
    std::function<juce::String (int)>  fxNameOf;    // indice -> nome exibido

    void setIndexInfo (int padNumber, const juce::String& keyHint)
    {
        index = padNumber;
        hint  = keyHint;
        repaint();
    }

    void setFileName (const juce::String& name)
    {
        fileName = name;
        repaint();
    }

    int  getAssignedFx() const noexcept { return assignedFx; }

    // Solta o efeito caso esteja ligado (ex.: ao trocar de efeito ou virar sample).
    void clearFxHeld()
    {
        if (fxHeld) { fxHeld = false; if (onFxHold && assignedFx >= 0) onFxHold (assignedFx, false); repaint(); }
    }

    void flash() noexcept { brightness = 1.0f; repaint(); }

    void decay() noexcept
    {
        if (brightness > 0.0f)
        {
            brightness = juce::jmax (0.0f, brightness - 0.12f);
            repaint();
        }
    }

    // --- Drag & drop de arquivos de audio (modo sample) ---
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)           override { dragOver = false; repaint(); }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        if (! files.isEmpty())
        {
            clearFxHeld();          // virar sample solta o efeito que estava preso
            assignedFx = -1;        // sample e FX sao mutuamente exclusivos
            if (onFileDropped)
                onFileDropped (juce::File (files[0]));
        }
        repaint();
    }

    // --- Drag & drop interno de um chip de efeito (modo FX) ---
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.isInt(); }
    void itemDragEnter (const SourceDetails&) override { fxOver = true;  repaint(); }
    void itemDragExit  (const SourceDetails&) override { fxOver = false; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        fxOver = false;
        clearFxHeld();                  // trocar de efeito nao deixa o anterior preso
        assignedFx = (int) d.description;
        if (onFxAssign) onFxAssign (assignedFx);
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (assignedFx >= 0) return;    // modo FX: a alternancia acontece no mouseUp
        flash();
        if (onTrigger)
            onTrigger();
    }

    // Modo FX: clique (sem arraste, ainda sobre o pad) liga/desliga o efeito.
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (assignedFx < 0) return;
        if (e.mouseWasDraggedSinceMouseDown()) return;
        if (! getLocalBounds().contains (e.getPosition())) return;

        fxHeld = ! fxHeld;
        if (onFxHold) onFxHold (assignedFx, fxHeld);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);

        if (assignedFx >= 0)
        {
            // --- Modo FX de voz ---
            auto base = juce::Colour (SynthLookAndFeel::accent2).withMultipliedBrightness (0.85f);
            if (fxHeld)
                base = juce::Colour (SynthLookAndFeel::accent);
            g.setColour (base);
            g.fillRoundedRectangle (r, 6.0f);

            g.setColour ((fxOver || dragOver) ? juce::Colour (SynthLookAndFeel::accent)
                                              : juce::Colour (0xff3a3f47));
            g.drawRoundedRectangle (r, 6.0f, (fxOver || dragOver) ? 2.5f : 1.0f);

            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
            g.drawText (juce::String (index) + "  " + hint, r.reduced (6.0f).removeFromTop (16.0f),
                        juce::Justification::topLeft);

            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
            g.drawFittedText ("FX: " + (fxNameOf ? fxNameOf (assignedFx) : juce::String (assignedFx)),
                              r.reduced (6.0f).withTrimmedTop (16.0f).toNearestInt(),
                              juce::Justification::centredBottom, 2);
            return;
        }

        // --- Modo sample ---
        const bool loaded = fileName.isNotEmpty();

        // Corpo
        auto base = juce::Colour (loaded ? SynthLookAndFeel::accent2 : SynthLookAndFeel::panel);
        if (brightness > 0.0f)
            base = base.interpolatedWith (juce::Colour (SynthLookAndFeel::accent), brightness);
        g.setColour (base);
        g.fillRoundedRectangle (r, 6.0f);

        // Borda (destaca quando arrastando um arquivo OU um chip de FX por cima)
        g.setColour ((dragOver || fxOver) ? juce::Colour (SynthLookAndFeel::accent)
                                          : juce::Colour (0xff3a3f47));
        g.drawRoundedRectangle (r, 6.0f, (dragOver || fxOver) ? 2.5f : 1.0f);

        // Numero do pad + tecla
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
        g.drawText (juce::String (index) + "  " + hint, r.reduced (6.0f).removeFromTop (16.0f),
                    juce::Justification::topLeft);

        // Nome do arquivo (ou dica)
        g.setColour (loaded ? juce::Colours::white.withAlpha (0.9f)
                            : juce::Colours::white.withAlpha (0.4f));
        g.setFont (11.0f);
        g.drawFittedText (loaded ? fileName : "arraste audio ou FX",
                          r.reduced (6.0f).withTrimmedTop (16.0f).toNearestInt(),
                          juce::Justification::centredBottom, 2);
    }

private:
    int          index = 0;
    juce::String hint, fileName;
    float        brightness = 0.0f;
    bool         dragOver = false;  // arrastando arquivo de audio por cima
    bool         fxOver   = false;  // arrastando chip de FX por cima

    int  assignedFx = -1;           // -1 = pad de sample; >=0 = pad de FX de voz
    bool fxHeld     = false;
};
