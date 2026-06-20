#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SynthLookAndFeel.h"

/**
    Um pad on-screen: alvo de drag&drop de arquivos de audio, clicavel para tocar,
    com indicador visual de disparo (acende ao tocar via mouse ou MIDI).
*/
class PadComponent : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    std::function<void()>                  onTrigger;     // clique no pad
    std::function<void (const juce::File&)> onFileDropped; // arquivo solto no pad

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

    void flash() noexcept { brightness = 1.0f; repaint(); }

    void decay() noexcept
    {
        if (brightness > 0.0f)
        {
            brightness = juce::jmax (0.0f, brightness - 0.12f);
            repaint();
        }
    }

    // --- Drag & drop ---
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)           override { dragOver = false; repaint(); }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragOver = false;
        if (onFileDropped && ! files.isEmpty())
            onFileDropped (juce::File (files[0]));
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        flash();
        if (onTrigger)
            onTrigger();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const bool loaded = fileName.isNotEmpty();

        // Corpo
        auto base = juce::Colour (loaded ? SynthLookAndFeel::accent2 : SynthLookAndFeel::panel);
        if (brightness > 0.0f)
            base = base.interpolatedWith (juce::Colour (SynthLookAndFeel::accent), brightness);
        g.setColour (base);
        g.fillRoundedRectangle (r, 6.0f);

        // Borda (destaca quando arrastando um arquivo por cima)
        g.setColour (dragOver ? juce::Colour (SynthLookAndFeel::accent)
                              : juce::Colour (0xff3a3f47));
        g.drawRoundedRectangle (r, 6.0f, dragOver ? 2.5f : 1.0f);

        // Numero do pad + tecla
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
        g.drawText (juce::String (index) + "  " + hint, r.reduced (6.0f).removeFromTop (16.0f),
                    juce::Justification::topLeft);

        // Nome do arquivo (ou dica)
        g.setColour (loaded ? juce::Colours::white.withAlpha (0.9f)
                            : juce::Colours::white.withAlpha (0.4f));
        g.setFont (11.0f);
        g.drawFittedText (loaded ? fileName : "arraste um audio",
                          r.reduced (6.0f).withTrimmedTop (16.0f).toNearestInt(),
                          juce::Justification::centredBottom, 2);
    }

private:
    int          index = 0;
    juce::String hint, fileName;
    float        brightness = 0.0f;
    bool         dragOver = false;
};
