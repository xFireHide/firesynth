#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "SynthLookAndFeel.h"

/**
    Roda vertical estilo pitch/modulation wheel (como no AMW Mini 32p):
    corpo metálico com ranhuras, indicador da posição e rótulo embaixo.

    Valor 0..1 (embaixo = 0, topo = 1). Arrastável; reflete tambem um valor
    externo (vindo do MIDI) via setValueExternally(), ignorado enquanto se arrasta.
*/
class WheelComponent : public juce::Component
{
public:
    std::function<void (float)> onValueChange;

    void setLabel (const juce::String& t) { label = t; repaint(); }

    void setValueExternally (float v)
    {
        if (dragging) return;
        v = juce::jlimit (0.0f, 1.0f, v);
        if (std::abs (v - value) > 1.0e-4f) { value = v; repaint(); }
    }

    float getValue() const noexcept { return value; }

    void mouseDown (const juce::MouseEvent& e) override { dragging = true; updateFromMouse (e); }
    void mouseDrag (const juce::MouseEvent& e) override { updateFromMouse (e); }
    void mouseUp   (const juce::MouseEvent&)   override { dragging = false; }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        auto labelArea = bounds.removeFromBottom (16);
        auto wheel = bounds.reduced (3).toFloat();
        const float radius = 5.0f;

        // Corpo metálico: gradiente horizontal (brilho no centro).
        juce::ColourGradient grad (juce::Colour (0xff9a9aa0), wheel.getX(), 0.0f,
                                   juce::Colour (0xff6e6e74), wheel.getRight(), 0.0f, false);
        grad.addColour (0.45, juce::Colour (0xfff4f4f6));
        grad.addColour (0.55, juce::Colour (0xffe2e2e6));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (wheel, radius);

        // Ranhuras horizontais (textura da roda).
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (float y = wheel.getY() + 4.0f; y < wheel.getBottom() - 2.0f; y += 5.0f)
            g.drawHorizontalLine ((int) y, wheel.getX() + 2.0f, wheel.getRight() - 2.0f);

        // Indicador da posição atual.
        const float y = wheel.getBottom() - value * wheel.getHeight();
        g.setColour (juce::Colour (SynthLookAndFeel::accent));
        g.fillRect (wheel.getX(), y - 1.5f, wheel.getWidth(), 3.0f);

        // Borda.
        g.setColour (juce::Colour (0xff3a3a40));
        g.drawRoundedRectangle (wheel, radius, 1.2f);

        // Rótulo.
        g.setColour (juce::Colour (SynthLookAndFeel::textCol));
        g.setFont (11.0f);
        g.drawText (label, labelArea, juce::Justification::centred);
    }

private:
    void updateFromMouse (const juce::MouseEvent& e)
    {
        auto wheel = getLocalBounds().withTrimmedBottom (16).reduced (3).toFloat();
        const float v = juce::jlimit (0.0f, 1.0f,
                                      (wheel.getBottom() - (float) e.position.y) / wheel.getHeight());
        value = v;
        repaint();
        if (onValueChange)
            onValueChange (v);
    }

    juce::String label;
    float value = 0.0f;
    bool  dragging = false;
};
