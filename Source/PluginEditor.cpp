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

    // The lower third of the readout window, freed when the placeholder
    // lettering came out. Inside the nameplate's glass frame with the same
    // 12-ish units of clearance the text above it has.
    static const juce::Rectangle<int> presetButton { 280, 254, 80, 26 };

    static const int  smallY    = 368;
    static const int  smallSize = 64;
    static const int  smallX[5] { 68, 178, 288, 398, 508 };
}

YellowcoatEditor::YellowcoatEditor (YellowcoatProcessor& p)
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

    preset.setTooltip ("Presets: bundled, yours, and save");
    preset.setClickingTogglesState (false);
    preset.getProperties().set ("plain", true);   // a plate, not a lamp
    preset.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (preset);

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

YellowcoatEditor::~YellowcoatEditor()
{
    setLookAndFeel (nullptr);
}

void YellowcoatEditor::configure (juce::Slider& s, const char* paramID, const juce::String& readoutName)
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

void YellowcoatEditor::flash (const juce::String& name, const juce::String& value)
{
    flashText  = name + "\n" + value;
    flashUntil = juce::Time::getMillisecondCounter() + 1400;
}

juce::Rectangle<int> YellowcoatEditor::d (int x, int y, int w, int h) const noexcept
{
    const float k = scale();
    return juce::Rectangle<int> (juce::roundToInt (x * k), juce::roundToInt (y * k),
                                 juce::roundToInt (w * k), juce::roundToInt (h * k));
}

juce::Rectangle<int> YellowcoatEditor::d (juce::Rectangle<int> r) const noexcept
{
    return d (r.getX(), r.getY(), r.getWidth(), r.getHeight());
}

void YellowcoatEditor::paint (juce::Graphics& g)
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

    // 18 units of side padding, not 10. The nameplate's glass frame is inset 6
    // from its own edge, so at 10 the longest strings — "1960s > 1950s", or the
    // era name when it arrives on the second line — came within 4 units of it
    // and read as overflowing. At 18 the tightest case clears by 13.
    auto text = plate.reduced (juce::roundToInt (18 * k), juce::roundToInt (26 * k));

    // Phosphor green, and brighter for the moment after you touch something —
    // the window is the one part of the panel that is lit rather than painted.
    g.setColour (flashing ? Palette::crt.brighter (0.30f) : Palette::crt);
    g.setFont (YellowcoatLookAndFeel::stencil (17.0f * k, true));
    g.drawFittedText (line1, text.removeFromTop (juce::roundToInt (56 * k)),
                      juce::Justification::centred, 2, 0.7f);

    // Two lines, like the first. Touching VINTAGE puts an era name down here,
    // and on one line the narrower box truncates it to an ellipsis.
    g.setColour (Palette::crt.withAlpha (0.72f));
    g.setFont (YellowcoatLookAndFeel::stencil (14.0f * k));
    g.drawFittedText (line2, text.removeFromTop (juce::roundToInt (40 * k)),
                      juce::Justification::centred, 2, 0.7f);
}

void YellowcoatEditor::resized()
{
    vintage.setBounds (d (Layout::knobVintage));
    size.setBounds    (d (Layout::knobSize));

    ptt.setBounds    (d (Layout::switchPtt));
    chime.setBounds  (d (Layout::switchChime));
    preset.setBounds (d (Layout::presetButton));

    juce::Slider* small[5] { &drive, &howl, &room, &mix, &output };

    for (int i = 0; i < 5; ++i)
        small[i]->setBounds (d (Layout::smallX[i], Layout::smallY,
                                Layout::smallSize, Layout::smallSize));
}

void YellowcoatEditor::timerCallback()
{
    // The chime lamp follows the parameter, not the mouse, so host automation
    // lights it too.
    if (auto* param = proc.apvts.getRawParameterValue (ParamID::chime))
    {
        const bool lit = param->load() > 0.5f;

        if (lit != chime.getToggleState())
            chime.setToggleState (lit, juce::dontSendNotification);
    }

    if (proc.presets.revision() != lastPresetRevision)
    {
        const bool first = lastPresetRevision < 0;
        lastPresetRevision = proc.presets.revision();

        if (! first && proc.presets.currentName().isNotEmpty())
            flash ("PRESET", proc.presets.currentName());
    }

    repaint (d (Layout::nameplate));
}

void YellowcoatEditor::showPresetMenu()
{
    // Rescanned every time it opens, so a file dropped into the folder from
    // Explorer appears without reopening the plugin.
    proc.presets.refresh();

    const auto current = proc.presets.currentName();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);

    const auto& bundled = proc.presets.bundled();
    const auto& user    = proc.presets.user();

    if (! bundled.isEmpty())
    {
        menu.addSectionHeader ("BUNDLED");

        for (int i = 0; i < bundled.size(); ++i)
            menu.addItem (bundledBase + i, bundled.getReference (i).name,
                          true, bundled.getReference (i).name == current);
    }

    if (! user.isEmpty())
    {
        menu.addSeparator();
        menu.addSectionHeader ("YOURS");

        for (int i = 0; i < user.size(); ++i)
            menu.addItem (userBase + i, user.getReference (i).name,
                          true, user.getReference (i).name == current);
    }

    menu.addSeparator();
    menu.addItem (cmdSave, "Save current settings as...");
    menu.addItem (cmdFolder, "Open presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (preset),
                        [this] (int result)
                        {
                            if (result == 0)
                                return;

                            if (result == cmdSave)
                            {
                                promptForPresetName();
                            }
                            else if (result == cmdFolder)
                            {
                                auto dir = PresetManager::folder();
                                dir.createDirectory();       // may not exist yet
                                dir.revealToUser();
                            }
                            else if (result >= userBase)
                            {
                                proc.presets.load (proc.presets.user().getReference (result - userBase));
                            }
                            else if (result >= bundledBase)
                            {
                                proc.presets.loadBundled (result - bundledBase);
                            }
                        });
}

void YellowcoatEditor::promptForPresetName()
{
    auto* window = new juce::AlertWindow ("Save preset",
                                          "Saved into Documents\\Yellowcoat\\Presets.",
                                          juce::MessageBoxIconType::NoIcon, this);

    window->addTextEditor ("name", proc.presets.currentName(), "Name:");
    window->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, window] (int result)
        {
            if (result == 1)
            {
                const auto name = window->getTextEditorContents ("name");

                if (proc.presets.saveAs (name) == juce::File())
                    flash ("SAVE", "FAILED");
                else
                    flash ("SAVED", name);
            }
        }), true);
}
