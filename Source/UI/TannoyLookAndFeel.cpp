#include "TannoyLookAndFeel.h"

TannoyLookAndFeel::TannoyLookAndFeel()
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

juce::Font TannoyLookAndFeel::stencil (float height, bool bold)
{
   #if JUCE_MAJOR_VERSION >= 8
    auto f = juce::Font (juce::FontOptions().withHeight (height));
   #else
    auto f = juce::Font (height);
   #endif

    f = f.withExtraKerningFactor (0.09f);
    return bold ? f.boldened() : f;
}

juce::Font TannoyLookAndFeel::getLabelFont (juce::Label& l)
{
    return stencil (l.getHeight() * 0.72f);
}

void TannoyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
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
    const auto  sq     = square.toFloat();
    const auto  centre = sq.getCentre();
    const float r      = sq.getWidth() * 0.5f;
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
        body->drawWithin (g, sq.reduced (r * 0.04f), juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        g.setColour (Palette::ink);
        g.fillEllipse (sq.reduced (r * 0.06f));
    }

    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

        if (pointer != nullptr)
        {
            pointer->drawWithin (g, sq, juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            g.setColour (Palette::cream);
            g.fillRect (juce::Rectangle<float> (centre.x - r * 0.03f, centre.y - r * 0.86f,
                                                r * 0.06f, r * 0.34f));
        }
    }
}
