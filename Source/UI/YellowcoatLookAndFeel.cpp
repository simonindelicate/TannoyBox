#include "YellowcoatLookAndFeel.h"

YellowcoatLookAndFeel::YellowcoatLookAndFeel()
{
    // Filmstrips first. If these turn out to be single square frames, or SVGs,
    // isStrip() stays false and we fall through to the face + pointer path.
    largeStrip.load ("knob_large");
    smallStrip.load ("knob_small");

    largeFace = Assets::loadDrawable ("knob_large");
    smallFace = Assets::loadDrawable ("knob_small");
    pointer   = Assets::loadDrawable ("knob_pointer");

    setColour (juce::Label::textColourId, Palette::cream);
    setColour (juce::TooltipWindow::backgroundColourId, Palette::ink);
    setColour (juce::TooltipWindow::textColourId, Palette::cream);
}

juce::Font YellowcoatLookAndFeel::stencil (float height, bool bold)
{
   #if JUCE_MAJOR_VERSION >= 8
    auto f = juce::Font (juce::FontOptions().withHeight (height));
   #else
    auto f = juce::Font (height);
   #endif

    f = f.withExtraKerningFactor (0.09f);
    return bold ? f.boldened() : f;
}

juce::Font YellowcoatLookAndFeel::getLabelFont (juce::Label& l)
{
    return stencil (l.getHeight() * 0.72f);
}

void YellowcoatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    const bool  isSmall = width < 100;
    const auto& strip   = isSmall ? smallStrip : largeStrip;

    const auto bounds = juce::Rectangle<int> (x, y, width, height);
    const int  side   = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto square = bounds.withSizeKeepingCentre (side, side);

    // ---- filmstrip: the artwork is the whole control -----------------------
    if (strip.isStrip())
    {
        strip.draw (g, square, sliderPos);
        return;
    }

    // ---- otherwise: engraved marks, value arc, face, rotated pointer -------
    // The marks and the arc live outside the knob body — the furthest is the
    // index marks at 1.13r — so the body has to be smaller than the component
    // to leave room for them. Derive r from the whole square instead and the
    // ring is drawn beyond the component's bounds, where JUCE clips it: not
    // at the corners, which have 1.41r to spare, but at the four points where
    // the arc crosses an edge.
    const auto  sq     = square.toFloat();
    const auto  centre = sq.getCentre();
    const float r      = sq.getWidth() * 0.5f / ringRoom;
    const auto  face   = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);

    const int detents = (int) slider.getProperties().getWithDefault ("detents", 0);
    if (detents > 1)
    {
        g.setColour (Palette::cream.withAlpha (0.55f));

        for (int i = 0; i < detents; ++i)
        {
            const float t = (float) i / (float) (detents - 1);
            const float a = startAngle + t * (endAngle - startAngle);
            const juce::Point<float> p1 { centre.x + std::sin (a) * r * 1.03f,
                                          centre.y - std::cos (a) * r * 1.03f };
            const juce::Point<float> p2 { centre.x + std::sin (a) * r * 1.13f,
                                          centre.y - std::cos (a) * r * 1.13f };
            g.drawLine ({ p1, p2 }, r * 0.030f);
        }
    }

    {
        juce::Path track, value;
        track.addCentredArc (centre.x, centre.y, r * 1.08f, r * 1.08f, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (centre.x, centre.y, r * 1.08f, r * 1.08f, 0.0f, startAngle, angle, true);

        g.setColour (Palette::ink.withAlpha (0.6f));
        g.strokePath (track, juce::PathStrokeType (r * 0.055f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (Palette::rust);
        g.strokePath (value, juce::PathStrokeType (r * 0.055f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    if (auto* body = isSmall ? smallFace.get() : largeFace.get())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        body->drawWithin (g, face.reduced (r * 0.04f), juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        g.setColour (Palette::ink);
        g.fillEllipse (face.reduced (r * 0.06f));
    }

    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

        if (pointer != nullptr)
        {
            pointer->drawWithin (g, face, juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            g.setColour (Palette::cream);
            g.fillRect (juce::Rectangle<float> (centre.x - r * 0.03f, centre.y - r * 0.86f,
                                                r * 0.06f, r * 0.34f));
        }
    }
}

void YellowcoatLookAndFeel::drawSwitch (juce::Graphics& g, juce::Rectangle<float> bounds,
                                    const juce::String& text, bool lit,
                                    bool highlighted, bool down)
{
    const float radius = juce::jmin (7.0f, bounds.getHeight() * 0.24f);

    // recess the switch sits in
    g.setColour (Palette::ink.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (Palette::moss.withAlpha (highlighted ? 0.55f : 0.30f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

    // the cap, pressed in slightly while held
    auto cap = bounds.reduced (bounds.getHeight() * 0.15f);
    g.setColour (down ? Palette::panel.darker (0.25f) : Palette::panel.brighter (0.06f));
    g.fillRoundedRectangle (cap, radius * 0.7f);
    g.setColour (Palette::ink.withAlpha (0.45f));
    g.drawRoundedRectangle (cap.reduced (0.5f), radius * 0.7f, 1.0f);

    cap = cap.reduced (cap.getHeight() * 0.16f);

    // indicator lamp
    auto lamp = cap.removeFromLeft (cap.getHeight());
    g.setColour (lit ? Palette::rust.brighter (0.30f) : Palette::ink.brighter (0.06f));
    g.fillEllipse (lamp.reduced (lamp.getWidth() * 0.20f));

    if (lit)
    {
        g.setColour (Palette::rust.withAlpha (0.30f));
        g.drawEllipse (lamp.reduced (lamp.getWidth() * 0.06f), lamp.getWidth() * 0.12f);
    }

    g.setColour (Palette::cream.withAlpha (lit ? 1.0f : 0.70f));
    g.setFont (stencil (juce::jmax (7.0f, cap.getHeight() * 0.62f), true));
    g.drawFittedText (text, cap.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void YellowcoatLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                          bool highlighted, bool down)
{
    drawSwitch (g, b.getLocalBounds().toFloat(), b.getButtonText(),
                b.getToggleState(), highlighted, down);
}

void YellowcoatLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                              const juce::Colour&, bool highlighted, bool down)
{
    // Momentary buttons have no toggle state of their own; CHIME's is driven
    // from its parameter so host automation lights the lamp too.
    drawSwitch (g, b.getLocalBounds().toFloat(), b.getButtonText(),
                b.getToggleState() || down, highlighted, down);
}
