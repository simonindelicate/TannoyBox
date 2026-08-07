#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Assets.h"

/** A vertically stacked sprite sheet of square frames. Frame count is inferred
    from the proportions, so no configuration is needed anywhere. */
struct Filmstrip
{
    juce::Image image;
    int frames = 1;

    void load (const juce::String& stem)
    {
        image = Assets::loadImage (stem);

        if (image.isValid() && image.getWidth() > 0)
            frames = juce::jmax (1, image.getHeight() / image.getWidth());
    }

    bool isStrip() const noexcept { return image.isValid() && frames > 1; }

    void draw (juce::Graphics& g, juce::Rectangle<int> area, float pos01) const
    {
        if (! image.isValid())
            return;

        const int frameHeight = image.getHeight() / frames;
        const int index = juce::jlimit (0, frames - 1,
                                        juce::roundToInt (pos01 * (float) (frames - 1)));

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (image,
                     area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                     0, index * frameHeight, image.getWidth(), frameHeight);
    }
};

/** Draws rotaries either from a filmstrip or from a static face plus a rotated
    pointer, whichever the artwork turns out to be. Set the slider property
    "detents" to an integer for that many index marks; they are suppressed in
    filmstrip mode on the assumption the artwork already has them. */
class TannoyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TannoyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;

    static juce::Font stencil (float height, bool bold = false);

private:
    Filmstrip largeStrip, smallStrip;
    std::unique_ptr<juce::Drawable> largeFace, smallFace, pointer;
};
