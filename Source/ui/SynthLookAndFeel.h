#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    Tema escuro coeso + desenho customizado dos knobs rotativos.
    Paleta: fundo grafite, acento ambar, traços claros.
*/
class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr juce::uint32 bg       = 0xff1a1c20;
    static constexpr juce::uint32 panel    = 0xff24272d;
    static constexpr juce::uint32 accent   = 0xfff2a541; // ambar
    static constexpr juce::uint32 accent2  = 0xff3fb2c4; // teal
    static constexpr juce::uint32 textCol  = 0xffe6e8ea;

    SynthLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (bg));
        setColour (juce::Slider::textBoxTextColourId,         juce::Colour (textCol));
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId,                 juce::Colour (textCol));
        setColour (juce::ComboBox::backgroundColourId,        juce::Colour (panel));
        setColour (juce::ComboBox::textColourId,              juce::Colour (textCol));
        setColour (juce::ComboBox::outlineColourId,           juce::Colour (0xff3a3f47));
        setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (panel));
        setColour (juce::TextButton::textColourOffId,         juce::Colour (textCol));
        setColour (juce::TextButton::buttonColourId,          juce::Colour (panel));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float track = radius * 0.78f;

        // Trilha de fundo
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff3a3f47));
        g.strokePath (bgArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Arco de valor
        juce::Path valArc;
        valArc.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (accent));
        g.strokePath (valArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Corpo do knob
        const float knobR = radius * 0.6f;
        g.setColour (juce::Colour (panel).brighter (0.15f));
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (juce::Colour (0xff15171a));
        g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.5f);

        // Ponteiro
        juce::Point<float> tip (centre.x + std::sin (angle) * knobR * 0.95f,
                                centre.y - std::cos (angle) * knobR * 0.95f);
        g.setColour (juce::Colour (accent));
        g.drawLine ({ centre, tip }, 2.5f);
    }
};
