#include "PluginProcessor.h"
#include "PluginEditor.h"

TannoyBoxProcessor::TannoyBoxProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout TannoyBoxProcessor::createLayout()
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

    return layout;
}

bool TannoyBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (in != out && in != juce::AudioChannelSet::mono())
        return false;

    return ! in.isDisabled();
}

void TannoyBoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    horn.prepare (sampleRate, samplesPerBlock);
    space.prepare (sampleRate, samplesPerBlock);

    dryBuffer.setSize (2, samplesPerBlock);
    monoBuffer.setSize (1, samplesPerBlock);
    wetBuffer.setSize (2, samplesPerBlock);

    for (auto* s : { &mixSm, &outSm, &driveSm, &howlSm, &vintageSm, &sizeSm })
        s->reset (sampleRate, 0.05);

    setLatencySamples (horn.getLatencySamples());
}

void TannoyBoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    vintageSm.setTargetValue (vintage);
    sizeSm.setTargetValue (size);
    driveSm.setTargetValue (driveDb);
    howlSm.setTargetValue (howl);
    mixSm.setTargetValue (mix);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (outDb));

    // Coefficients update once per block from the smoothed value: cheap, and
    // fast enough that a dial sweep sounds continuous rather than stepped.
    horn.setParameters (vintageSm.skip (n), driveSm.skip (n), howlSm.skip (n));
    space.setSize (sizeSm.skip (n));

    // ---- keep a dry copy ----------------------------------------------------
    dryBuffer.setSize (juce::jmax (1, numOut), n, false, false, true);
    for (int ch = 0; ch < numOut; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, juce::jmin (ch, numIn - 1), 0, n);

    // ---- sum to mono: a PA is one amplifier feeding one horn ----------------
    monoBuffer.setSize (1, n, false, false, true);
    monoBuffer.clear();
    for (int ch = 0; ch < numIn; ++ch)
        monoBuffer.addFrom (0, 0, buffer, ch, 0, n, numIn > 1 ? 0.5f : 1.0f);

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

juce::AudioProcessorEditor* TannoyBoxProcessor::createEditor()
{
    return new TannoyBoxEditor (*this);
}

void TannoyBoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TannoyBoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TannoyBoxProcessor();
}
