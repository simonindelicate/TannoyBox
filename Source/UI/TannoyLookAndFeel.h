#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Assets.h"

/** Draws every rotary from two drawables: a static face and a pointer that is
    rotated about the centre of its own frame. Set the slider property
    "detents" to an integer to get that many index marks around the arc. */
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
    std::unique_ptr<juce::Drawable> face, smallFace, pointer;
};
