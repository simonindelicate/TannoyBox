#pragma once

#include "PluginProcessor.h"
#include "UI/YellowcoatLookAndFeel.h"

class YellowcoatEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit YellowcoatEditor (YellowcoatProcessor&);
    ~YellowcoatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configure (juce::Slider&, const char* paramID, const juce::String& readoutName);
    void flash (const juce::String& name, const juce::String& value);

    /** design space (640 x 480) -> current window */
    juce::Rectangle<int> d (int x, int y, int w, int h) const noexcept;
    juce::Rectangle<int> d (juce::Rectangle<int> r) const noexcept;
    float scale() const noexcept { return (float) getWidth() / 640.0f; }

    YellowcoatProcessor& proc;
    YellowcoatLookAndFeel lnf;
    juce::TooltipWindow tips { this, 700 };

    std::unique_ptr<juce::Drawable> background, logo, nameplate;

    juce::Slider vintage, size, drive, howl, room, mix, output;
    juce::ToggleButton ptt { "PTT" };
    juce::TextButton   chime { "CHIME" };

    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pttAttachment;

    // CHIME is momentary, so it drives its parameter by hand rather than
    // through an attachment: down on press, up on release.
    bool chimeDown = false;

    juce::String flashText;
    juce::uint32 flashUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YellowcoatEditor)
};
