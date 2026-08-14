#include "PluginProcessor.h"
#include "PluginEditor.h"

YellowcoatProcessor::YellowcoatProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout YellowcoatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::vintage, 1 }, "Vintage",
        NormalisableRange<float> { 0.0f, 1.0f }, 0.72f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return eraDisplayName (v); })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::size, 1 }, "Size",
        NormalisableRange<float> { 0.0f, 1.0f }, 0.40f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (roundToInt (5.0f + 110.0f * v)) + " m"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::drive, 1 }, "Drive",
        NormalisableRange<float> { -12.0f, 12.0f, 0.1f }, 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::howl, 1 }, "Howl",
        NormalisableRange<float> { 0.0f, 1.0f }, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::mix, 1 }, "Mix",
        NormalisableRange<float> { 0.0f, 1.0f }, 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::output, 1 }, "Output",
        NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    // Appended rather than inserted: hosts that automate by index rather than
    // by ID keep working on sessions saved before these existed.
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::room, 1 }, "Room",
        NormalisableRange<float> { 0.0f, 1.0f }, 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int)
            {
                if (v < 0.005f) return String ("HORN ONLY");
                if (v > 0.995f) return String ("ROOM ONLY");
                return String (roundToInt (v * 100.0f)) + " %";
            })));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::ptt, 1 }, "PTT Gate", false));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::chime, 1 }, "Chime", false));

    return layout;
}

bool YellowcoatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (in != out && in != juce::AudioChannelSet::mono())
        return false;

    return ! in.isDisabled();
}

void YellowcoatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    horn.prepare (sampleRate, samplesPerBlock);
    space.prepare (sampleRate, samplesPerBlock);
    chime.prepare (sampleRate);

    dryBuffer.setSize (2, samplesPerBlock);
    monoBuffer.setSize (1, samplesPerBlock);
    wetBuffer.setSize (2, samplesPerBlock);

    for (auto* s : { &mixSm, &outSm, &driveSm, &howlSm, &vintageSm, &sizeSm, &roomSm })
        s->reset (sampleRate, 0.05);

    // Seeded from where the dials actually are, not from zero. Without this the
    // first 50 ms of every transport start ramps OUTPUT up from silence and
    // sweeps VINTAGE across from MODERN.
    const auto now = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    vintageSm.setCurrentAndTargetValue (now (ParamID::vintage));
    sizeSm.setCurrentAndTargetValue    (now (ParamID::size));
    driveSm.setCurrentAndTargetValue   (now (ParamID::drive));
    howlSm.setCurrentAndTargetValue    (now (ParamID::howl));
    mixSm.setCurrentAndTargetValue     (now (ParamID::mix));
    roomSm.setCurrentAndTargetValue    (now (ParamID::room));
    outSm.setCurrentAndTargetValue     (juce::Decibels::decibelsToGain (now (ParamID::output)));

    space.setRoomAmount (roomSm.getCurrentValue());

    // Seeded, not cleared: a session saved with the button held would otherwise
    // look like a rising edge and strike the chime on load.
    lastChime = now (ParamID::chime) > 0.5f;

    setLatencySamples (horn.getLatencySamples());
}

void YellowcoatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n        = buffer.getNumSamples();
    const int numIn    = getTotalNumInputChannels();
    const int numOut   = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    if (n == 0)
        return;

    // ---- parameters ---------------------------------------------------------
    const float vintage = apvts.getRawParameterValue (ParamID::vintage)->load();
    const float size    = apvts.getRawParameterValue (ParamID::size)->load();
    const float driveDb = apvts.getRawParameterValue (ParamID::drive)->load();
    const float howl    = apvts.getRawParameterValue (ParamID::howl)->load();
    const float mix     = apvts.getRawParameterValue (ParamID::mix)->load();
    const float outDb   = apvts.getRawParameterValue (ParamID::output)->load();
    const float room    = apvts.getRawParameterValue (ParamID::room)->load();
    const bool  pttOn   = apvts.getRawParameterValue (ParamID::ptt)->load() > 0.5f;
    const bool  chimeOn = apvts.getRawParameterValue (ParamID::chime)->load() > 0.5f;

    vintageSm.setTargetValue (vintage);
    sizeSm.setTargetValue (size);
    driveSm.setTargetValue (driveDb);
    howlSm.setTargetValue (howl);
    mixSm.setTargetValue (mix);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));
    roomSm.setTargetValue (room);

    // Coefficients update once per block from the smoothed value: cheap, and
    // fast enough that a dial sweep sounds continuous rather than stepped.
    horn.setParameters (vintageSm.skip (n), driveSm.skip (n), howlSm.skip (n), pttOn);
    space.setSize (sizeSm.skip (n));
    space.setRoomAmount (roomSm.skip (n));

    if (chimeOn && ! lastChime)
        chime.trigger();

    lastChime = chimeOn;

    // ---- keep a dry copy ----------------------------------------------------
    dryBuffer.setSize (juce::jmax (1, numOut), n, false, false, true);
    for (int ch = 0; ch < numOut; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, juce::jmin (ch, numIn - 1), 0, n);

    // ---- sum to mono: a PA is one amplifier feeding one horn ----------------
    monoBuffer.setSize (1, n, false, false, true);
    monoBuffer.clear();
    for (int ch = 0; ch < numIn; ++ch)
        monoBuffer.addFrom (0, 0, buffer, ch, 0, n, numIn > 1 ? 0.5f : 1.0f);

    // ---- chime, into the horn's input --------------------------------------
    // Deliberately upstream of everything: the horn's band-limiting is what
    // makes it a station chime rather than an orchestral one, and being real
    // programme material it keys the PTT gate open on its own.
    chime.process (monoBuffer.getWritePointer (0), n);

    // ---- horn ---------------------------------------------------------------
    juce::dsp::AudioBlock<float> monoBlock (monoBuffer);
    horn.process (monoBlock.getSubBlock (0, (size_t) n));

    // ---- room ---------------------------------------------------------------
    wetBuffer.setSize (2, n, false, false, true);
    space.process (monoBuffer.getReadPointer (0),
                   wetBuffer.getWritePointer (0),
                   wetBuffer.getWritePointer (1), n);

    // ---- blend --------------------------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        const float m = mixSm.getNextValue();
        const float g = outSm.getNextValue();

        for (int ch = 0; ch < numOut; ++ch)
        {
            const float dry = dryBuffer.getSample (ch, i);
            const float wet = wetBuffer.getSample (juce::jmin (ch, 1), i);
            buffer.setSample (ch, i, (dry * (1.0f - m) + wet * m) * g);
        }
    }
}

juce::AudioProcessorEditor* YellowcoatProcessor::createEditor()
{
    return new YellowcoatEditor (*this);
}

const juce::String YellowcoatProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, presets.bundled().size()))
        return presets.bundled().getReference (index).name;

    return "Default";
}

void YellowcoatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Carried alongside the parameters so the readout can say what you loaded
    // after a session reload. It is a label, not a source of truth — the
    // parameter values in the same tree are what actually restore the sound.
    state.setProperty ("presetName", presets.currentName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void YellowcoatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            presets.setCurrentName (tree.getProperty ("presetName", juce::String()).toString());
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YellowcoatProcessor();
}
