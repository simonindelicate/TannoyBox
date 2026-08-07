#pragma once

#include "PluginProcessor.h"
#include "UI/TannoyLookAndFeel.h"

class TannoyBoxEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit TannoyBoxEditor (TannoyBoxProcessor&);
    ~TannoyBoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configure (juce::Slider&, const char* paramID, const juce::String& readoutName);

    /** design space (640 x 480) -> current window */
    juce::Rectangle<int> d (int x, int y, int w, int h) const noexcept;
    float scale() const noexcept { return (float) getWidth() / 640.0f; }

    TannoyBoxProcessor& proc;
    TannoyLookAndFeel lnf;

    std::unique_ptr<juce::Drawable> background, logo, nameplate;

    juce::Slider vintage, size, drive, howl, mix, output;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;

    juce::String flashText;
    juce::uint32 flashUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TannoyBoxEditor)
};
