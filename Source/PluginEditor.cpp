#include "PluginEditor.h"

/*  Layout is authored in a 640 x 480 design space and scaled to the window, so
    the panel artwork and the controls always agree. Every rectangle below is in
    design units — if you move something in the background artwork, move it here
    too, and in Tools/make_background.py if you are still generating the SVG.

    Vertical rhythm, top to bottom. Everything here has at least 4 design units
    of air around it, which is what fixed the crowding when the fifth small dial
    and the two switches arrived:

        16  ..  68    logo, and the two switches level with it
                 86   header rule
        124 .. 304    two large dials and the readout; engraving runs to 346
                356   divider
        368 .. 432    five small dials on 110 centres; lettering to 456
                470   panel border

    The large dials are 180, not 196: at 196 the topmost index legend on SIZE
    collided with the header rule, and losing 16 units off the dials is less
    visible than lettering sitting on a line.                                   */

namespace Layout
{
    static const juce::Rectangle<int> logo        { 26,  16,  240, 52 };
    static const juce::Rectangle<int> switchPtt   { 436, 18,  80,  38 };
    static const juce::Rectangle<int> switchChime { 528, 18,  80,  38 };
    static const juce::Rectangle<int> nameplate   { 262, 124, 116, 180 };
    static const juce::Rectangle<int> knobVintage { 54,  124, 180, 180 };
    static const juce::Rectangle<int> knobSize    { 406, 124, 180, 180 };

    static const int  smallY    = 368;
    static const int  smallSize = 64;
    static const int  smallX[5] { 68, 178, 288, 398, 508 };
}

TannoyBoxEditor::TannoyBoxEditor (TannoyBoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    background = Assets::loadDrawable ("background");
    logo       = Assets::loadDrawable ("logo");
    nameplate  = Assets::loadDrawable ("nameplate");

    configure (vintage, ParamID::vintage, "VINTAGE");
    configure (size,    ParamID::size,    "SIZE");
    configure (drive,   ParamID::drive,   "DRIVE");
    configure (howl,    ParamID::howl,    "HOWL");
    configure (room,    ParamID::room,    "ROOM");
    configure (mix,     ParamID::mix,     "MIX");
    configure (output,  ParamID::output,  "OUTPUT");

    vintage.getProperties().set ("detents", 4);   // MODERN / 1970s / 1960s / 1950s
    size.getProperties().set    ("detents", 4);   // 5 / 40 / 80 / 115 m
    room.getProperties().set    ("detents", 3);   // horn only / balanced / room only

    ptt.setTooltip ("Keying gate: hiss and hum only while the channel is open,"
                    " with a key click and a release thump");
    addAndMakeVisible (ptt);
    pttAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, ParamID::ptt, ptt);

    ptt.onClick = [this] { flash ("PTT GATE", ptt.getToggleState() ? "ON" : "OFF"); };

    chime.setTooltip ("Four-note station chime through the horn");
    chime.setClickingTogglesState (false);
    chime.setTriggeredOnMouseDown (true);
    addAndMakeVisible (chime);

    // Held for as long as the mouse is down, so the audio thread is certain to
    // see the rising edge however the block size falls.
    chime.onStateChange = [this]
    {
        const bool down = chime.isDown();

        if (down == chimeDown)
            return;

        chimeDown = down;

        if (auto* param = proc.apvts.getParameter (ParamID::chime))
        {
            if (down)
                param->beginChangeGesture();

            param->setValueNotifyingHost (down ? 1.0f : 0.0f);

            if (! down)
                param->endChangeGesture();
        }

        if (down)
            flash ("CHIME", "STRUCK");
    };

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (640.0 / 480.0);

        // Upper limit is deliberately the 2x authoring size of the bitmap
        // artwork. Past that, PNGs are being upscaled and it shows.
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
        flash (readoutName, s.getTextFromValue (s.getValue()));
    };

    addAndMakeVisible (s);
    attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.apvts, paramID, s));
}

void TannoyBoxEditor::flash (const juce::String& name, const juce::String& value)
{
    flashText  = name + "\n" + value;
    flashUntil = juce::Time::getMillisecondCounter() + 1400;
}

juce::Rectangle<int> TannoyBoxEditor::d (int x, int y, int w, int h) const noexcept
{
    const float k = scale();
    return juce::Rectangle<int> (juce::roundToInt (x * k), juce::roundToInt (y * k),
                                 juce::roundToInt (w * k), juce::roundToInt (h * k));
}

juce::Rectangle<int> TannoyBoxEditor::d (juce::Rectangle<int> r) const noexcept
{
    return d (r.getX(), r.getY(), r.getWidth(), r.getHeight());
}

void TannoyBoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    if (background != nullptr)
        background->drawWithin (g, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit, 1.0f);

    if (logo != nullptr)
        logo->drawWithin (g, d (Layout::logo).toFloat(), juce::RectanglePlacement::centred, 1.0f);

    const auto plate = d (Layout::nameplate);

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
    vintage.setBounds (d (Layout::knobVintage));
    size.setBounds    (d (Layout::knobSize));

    ptt.setBounds   (d (Layout::switchPtt));
    chime.setBounds (d (Layout::switchChime));

    juce::Slider* small[5] { &drive, &howl, &room, &mix, &output };

    for (int i = 0; i < 5; ++i)
        small[i]->setBounds (d (Layout::smallX[i], Layout::smallY,
                                Layout::smallSize, Layout::smallSize));
}

void TannoyBoxEditor::timerCallback()
{
    // The chime lamp follows the parameter, not the mouse, so host automation
    // lights it too.
    if (auto* param = proc.apvts.getRawParameterValue (ParamID::chime))
    {
        const bool lit = param->load() > 0.5f;

        if (lit != chime.getToggleState())
            chime.setToggleState (lit, juce::dontSendNotification);
    }

    repaint (d (Layout::nameplate));
}
