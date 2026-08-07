#include "TannoyLookAndFeel.h"

TannoyLookAndFeel::TannoyLookAndFeel()
{
    face      = Assets::load ("knob_face.svg",       BinaryData::knob_face_svg,       BinaryData::knob_face_svgSize);
    smallFace = Assets::load ("knob_small_face.svg", BinaryData::knob_small_face_svg, BinaryData::knob_small_face_svgSize);
    pointer   = Assets::load ("knob_pointer.svg",    BinaryData::knob_pointer_svg,    BinaryData::knob_pointer_svgSize);

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
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto square = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), bounds.getHeight()),
                                                      juce::jmin (bounds.getWidth(), bounds.getHeight()));
    const auto centre = square.getCentre();
    const float r     = square.getWidth() * 0.5f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // ---- index marks --------------------------------------------------------
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

    // ---- value arc ----------------------------------------------------------
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

    // ---- face ---------------------------------------------------------------
    auto* body = (width < 100 && smallFace != nullptr) ? smallFace.get() : face.get();

    if (body != nullptr)
    {
        body->drawWithin (g, square.reduced (r * 0.04f), juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        g.setColour (Palette::ink);
        g.fillEllipse (square.reduced (r * 0.06f));
    }

    // ---- pointer ------------------------------------------------------------
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

        if (pointer != nullptr)
        {
            pointer->drawWithin (g, square, juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            g.setColour (Palette::cream);
            g.fillRect (juce::Rectangle<float> (centre.x - r * 0.03f, centre.y - r * 0.86f,
                                                r * 0.06f, r * 0.34f));
        }
    }
}
