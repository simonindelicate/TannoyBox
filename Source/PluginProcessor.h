#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/Chime.h"
#include "DSP/EraProfiles.h"
#include "DSP/HornBody.h"
#include "DSP/SpaceEngine.h"

/*  These strings are written into every saved session. Adding to this list is
    safe; renaming or removing anything in it is not. */
namespace ParamID
{
    static constexpr const char* vintage = "vintage";
    static constexpr const char* size    = "size";
    static constexpr const char* drive   = "drive";
    static constexpr const char* howl    = "howl";
    static constexpr const char* mix     = "mix";
    static constexpr const char* output  = "output";
    static constexpr const char* room    = "room";
    static constexpr const char* ptt     = "ptt";
    static constexpr const char* chime   = "chime";
}

class YellowcoatProcessor : public juce::AudioProcessor
{
public:
    YellowcoatProcessor();
    ~YellowcoatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "Yellowcoat"; }
    bool acceptsMidi() const override                        { return false; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 8.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "YELLOWCOAT", createLayout() };

private:
    HornBody    horn;
    SpaceEngine space;
    Chime       chime;

    juce::AudioBuffer<float> dryBuffer, monoBuffer, wetBuffer;
    juce::SmoothedValue<float> mixSm, outSm, driveSm, howlSm, vintageSm, sizeSm, roomSm;

    // The chime parameter is a momentary trigger: the phrase fires on the
    // rising edge, so holding it down does not machine-gun.
    bool lastChime = false;

    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YellowcoatProcessor)
};
