#include "PluginEditor.h"

/*  Layout is authored in a 640 x 480 design space and scaled to the window, so
    the panel artwork and the controls always agree. Every rectangle below is in
    design units — if you move something in background.svg, move it here too.   */

namespace Layout
{
    static const juce::Rectangle<int> logo        { 26,  20, 240,  52 };
    static const juce::Rectangle<int> nameplate   { 262, 120, 116, 180 };
    static const juce::Rectangle<int> knobVintage { 46,  116, 196, 196 };
    static const juce::Rectangle<int> knobSize    { 398, 116, 196, 196 };

    static const int  smallY    = 372;
    static const int  smallSize = 64;
    static const int  smallX[4] { 64, 213, 363, 512 };
}

TannoyBoxEditor::TannoyBoxEditor (TannoyBoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    background = Assets::load ("background.svg", BinaryData::background_svg, BinaryData::background_svgSize);
    logo       = Assets::load ("logo.svg",       BinaryData::logo_svg,       BinaryData::logo_svgSize);
    nameplate  = Assets::load ("nameplate.svg",  BinaryData::nameplate_svg,  BinaryData::nameplate_svgSize);

    configure (vintage, ParamID::vintage, "VINTAGE");
    configure (size,    ParamID::size,    "SIZE");
    configure (drive,   ParamID::drive,   "DRIVE");
    configure (howl,    ParamID::howl,    "HOWL");
    configure (mix,     ParamID::mix,     "MIX");
    configure (output,  ParamID::output,  "OUTPUT");

    vintage.getProperties().set ("detents", 4);   // MODERN / 1970s / 1960s / 1950s
    size.getProperties().set    ("detents", 5);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (640.0 / 480.0);
        c->setSizeLimits (480, 360, 1280, 960);
    }

    setSize (640, 480);
    startTimerHz (24);
}

TannoyBoxEditor::~TannoyBoxEditor()
{
    setLookAndFeel (nullptr);
}

void TannoyBoxEditor::configure (juce::Slider& s, const char* paramID, const juce::String& readoutName)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                           juce::MathConstants<float>::pi * 2.75f, true);
    s.setTooltip (readoutName);

    // Any control the user touches takes over the readout window for a moment.
    s.onValueChange = [this, &s, readoutName]
    {
        flashText  = readoutName + "\n" + s.getTextFromValue (s.getValue());
        flashUntil = juce::Time::getMillisecondCounter() + 1400;
    };

    addAndMakeVisible (s);
    attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.apvts, paramID, s));
}

juce::Rectangle<int> TannoyBoxEditor::d (int x, int y, int w, int h) const noexcept
{
    const float k = scale();
    return juce::Rectangle<int> (juce::roundToInt (x * k), juce::roundToInt (y * k),
                                 juce::roundToInt (w * k), juce::roundToInt (h * k));
}

void TannoyBoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);

    if (background != nullptr)
        background->drawWithin (g, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit, 1.0f);

    if (logo != nullptr)
        logo->drawWithin (g, d (Layout::logo.getX(), Layout::logo.getY(),
                                Layout::logo.getWidth(), Layout::logo.getHeight()).toFloat(),
                          juce::RectanglePlacement::centred, 1.0f);

    const auto plate = d (Layout::nameplate.getX(), Layout::nameplate.getY(),
                          Layout::nameplate.getWidth(), Layout::nameplate.getHeight());

    if (nameplate != nullptr)
        nameplate->drawWithin (g, plate.toFloat(), juce::RectanglePlacement::centred, 1.0f);

    // ---- readout window -----------------------------------------------------
    const bool flashing = juce::Time::getMillisecondCounter() < flashUntil;
    const float k = scale();

    juce::String line1, line2;

    if (flashing)
    {
        line1 = flashText.upToFirstOccurrenceOf ("\n", false, false);
        line2 = flashText.fromFirstOccurrenceOf ("\n", false, false);
    }
    else
    {
        line1 = eraDisplayName ((float) vintage.getValue());
        line2 = juce::String (juce::roundToInt (5.0 + 110.0 * size.getValue())) + " m";
    }

    auto text = plate.reduced (juce::roundToInt (10 * k), juce::roundToInt (26 * k));

    g.setColour (flashing ? Palette::cream : Palette::rust.brighter (0.35f));
    g.setFont (TannoyLookAndFeel::stencil (17.0f * k, true));
    g.drawFittedText (line1, text.removeFromTop (juce::roundToInt (56 * k)),
                      juce::Justification::centred, 2, 0.7f);

    g.setColour (Palette::cream.withAlpha (0.78f));
    g.setFont (TannoyLookAndFeel::stencil (14.0f * k));
    g.drawFittedText (line2, text.removeFromTop (juce::roundToInt (34 * k)),
                      juce::Justification::centred, 1, 0.7f);
}

void TannoyBoxEditor::resized()
{
    vintage.setBounds (d (Layout::knobVintage.getX(), Layout::knobVintage.getY(),
                          Layout::knobVintage.getWidth(), Layout::knobVintage.getHeight()));

    size.setBounds (d (Layout::knobSize.getX(), Layout::knobSize.getY(),
                       Layout::knobSize.getWidth(), Layout::knobSize.getHeight()));

    juce::Slider* small[4] { &drive, &howl, &mix, &output };

    for (int i = 0; i < 4; ++i)
        small[i]->setBounds (d (Layout::smallX[i], Layout::smallY,
                                Layout::smallSize, Layout::smallSize));
}

void TannoyBoxEditor::timerCallback()
{
    repaint (d (Layout::nameplate.getX(), Layout::nameplate.getY(),
                Layout::nameplate.getWidth(), Layout::nameplate.getHeight()));
}
