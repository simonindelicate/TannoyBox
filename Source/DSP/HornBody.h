#pragma once

#include <juce_dsp/juce_dsp.h>
#include "EraProfiles.h"
#include "PttGate.h"

/*  ============================================================================
    HornBody

    Mono. This is the loudspeaker itself: the signal chain of a re-entrant horn
    hanging off an overworked line amplifier.

        pre band-limit  ->  formant bells  ->  wobble  ->  [4x] saturate
        ->  line-amp limiter  ->  PTT gate  ->  howl resonance
        ->  hiss + hum  ->  key click / release thump
        ->  post band-limit  ->  DC block

    Band-limiting happens on both sides of the nonlinearity: once to feed the
    driver only what it can reproduce, once to stop the distortion products
    escaping outside the horn's passband. That two-sided arrangement is what
    stops it sounding like a fuzz pedal.

    The PTT gate sits where the keying relay would: after the amplifier, so it
    gates the howl loop and the noise floor along with the programme, and its
    own click and thump are still shaped by the post filter on the way out.
    ============================================================================
*/

class HornBody
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };

        for (auto* f : { &hpf, &lpf, &pk1, &pk2, &pk3, &postHpf, &postLpf, &howlPk })
            f->prepare (spec);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversampler->initProcessing ((size_t) maxBlockSize);

        wobbleDelay.prepare (spec);
        wobbleDelay.setMaximumDelayInSamples ((int) (0.02 * sampleRate) + 8);

        howlDelay.prepare (spec);
        howlDelay.setMaximumDelayInSamples ((int) (0.05 * sampleRate) + 8);
        howlDelaySamples = (float) (0.011 * sampleRate);

        attCoef = std::exp (-1.0f / (float) (0.004 * sampleRate));
        relCoef = std::exp (-1.0f / (float) (0.130 * sampleRate));

        ptt.prepare (sampleRate);

        lastVintage = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto* f : { &hpf, &lpf, &pk1, &pk2, &pk3, &postHpf, &postLpf, &howlPk })
            f->reset();

        if (oversampler != nullptr) oversampler->reset();
        wobbleDelay.reset();
        howlDelay.reset();
        ptt.reset();
        env = 0.0f; howlState = 0.0f; dcX1 = dcY1 = 0.0f;
        wobPhase = humPhase = 0.0f;
    }

    int getLatencySamples() const
    {
        return oversampler != nullptr ? juce::roundToInt (oversampler->getLatencyInSamples()) : 0;
    }

    /** Called once per block. Coefficients are only rebuilt when the dial moves. */
    void setParameters (float vintage01, float extraDriveDb, float howlAmount, bool pttEnabled)
    {
        drive     = juce::Decibels::decibelsToGain (extraDriveDb);
        howlAmt   = howlAmount;

        ptt.setEnabled (pttEnabled);

        if (std::abs (vintage01 - lastVintage) > 1.0e-4f)
        {
            lastVintage = vintage01;
            profile = interpolateEra (vintage01);

            using C = juce::dsp::IIR::Coefficients<float>;
            *hpf.coefficients     = *C::makeHighPass (sr, profile.hpfHz, profile.hpfQ);
            *lpf.coefficients     = *C::makeLowPass  (sr, profile.lpfHz, profile.lpfQ);
            *postHpf.coefficients = *C::makeHighPass (sr, profile.hpfHz * 0.92f, profile.hpfQ * 0.8f);
            *postLpf.coefficients = *C::makeLowPass  (sr, profile.lpfHz * 1.08f, profile.lpfQ * 0.8f);

            *pk1.coefficients = *C::makePeakFilter (sr, profile.f1.hz, profile.f1.q,
                                                    juce::Decibels::decibelsToGain (profile.f1.gainDb));
            *pk2.coefficients = *C::makePeakFilter (sr, profile.f2.hz, profile.f2.q,
                                                    juce::Decibels::decibelsToGain (profile.f2.gainDb));
            *pk3.coefficients = *C::makePeakFilter (sr, profile.f3.hz, profile.f3.q,
                                                    juce::Decibels::decibelsToGain (profile.f3.gainDb));

            // Feedback howl sits on the strongest formant — that is the note a
            // real PA picks when it rings.
            *howlPk.coefficients = *C::makePeakFilter (sr, profile.f2.hz, 12.0f, 4.0f);
            howlDelaySamples = juce::jlimit (8.0f, (float) (0.045 * sr),
                                             (float) (sr / juce::jmax (60.0f, profile.f2.hz * 0.11f)));

            eraDrive   = juce::Decibels::decibelsToGain (profile.driveDb);
            eraTrim    = juce::Decibels::decibelsToGain (-profile.driveDb * 0.62f);
            hissGain   = juce::Decibels::decibelsToGain (profile.hissDb);
            humGain    = juce::Decibels::decibelsToGain (profile.humDb);
            wobInc     = juce::MathConstants<float>::twoPi * profile.wobbleRateHz / (float) sr;
            humInc     = juce::MathConstants<float>::twoPi * 50.0f / (float) sr;
            compSlope  = 1.0f / juce::jmax (1.0f, profile.compRatio) - 1.0f;
        }
    }

    /** in-place on a single-channel block */
    void process (juce::dsp::AudioBlock<float> block)
    {
        auto* x = block.getChannelPointer (0);
        const int n = (int) block.getNumSamples();

        // ---- 1. pre band-limit, formants, mechanical wobble -----------------
        for (int i = 0; i < n; ++i)
        {
            float s = hpf.processSample (x[i]);
            s = lpf.processSample (s);
            s = pk1.processSample (s);
            s = pk2.processSample (s);
            s = pk3.processSample (s);

            wobPhase += wobInc;
            if (wobPhase > juce::MathConstants<float>::twoPi)
                wobPhase -= juce::MathConstants<float>::twoPi;

            const float lfo  = std::sin (wobPhase);
            const float lfo2 = std::sin (wobPhase * 1.37f + 1.1f);

            wobbleDelay.pushSample (0, s);
            const float base = (float) (0.006 * sr);
            s = wobbleDelay.popSample (0, base * (1.0f + profile.wobbleDepth * 0.55f * lfo), true);
            s *= 1.0f + profile.wobbleDepth * 0.35f * lfo2;

            x[i] = s * eraDrive * drive;
        }

        // ---- 2. oversampled nonlinearity ------------------------------------
        {
            auto up = oversampler->processSamplesUp (block);
            auto* o = up.getChannelPointer (0);
            const int m = (int) up.getNumSamples();

            for (int i = 0; i < m; ++i)
                o[i] = saturate (o[i], profile.asymmetry, profile.clipHardness);

            oversampler->processSamplesDown (block);
        }

        // ---- 3. limiter, howl, noise, post band-limit ------------------------
        for (int i = 0; i < n; ++i)
        {
            float s = x[i] * eraTrim;

            // line-amp limiting
            const float rect = std::abs (s);
            env = rect > env ? attCoef * env + (1.0f - attCoef) * rect
                             : relCoef * env + (1.0f - relCoef) * rect;

            const float envDb = juce::Decibels::gainToDecibels (env + 1.0e-9f);
            const float over  = envDb - profile.compThreshDb;
            if (over > 0.0f)
                s *= juce::Decibels::decibelsToGain (over * compSlope);

            // keying relay. Detects on the limited programme, which is the same
            // thing the real one would be watching, and returns 1 / 1 / 0 when
            // the switch is off.
            const auto key = ptt.tick (s);
            s *= key.gate;

            // feedback howl
            if (howlAmt > 0.001f)
            {
                const float d   = howlDelay.popSample (0, howlDelaySamples, true);
                const float res = std::tanh (howlPk.processSample (d) * 1.2f);
                howlDelay.pushSample (0, s + howlAmt * 0.85f * res);
                s += howlAmt * 0.55f * res;
            }
            else
            {
                howlDelay.pushSample (0, 0.0f);
                howlDelay.popSample (0, howlDelaySamples, true);
            }

            // hiss and mains buzz, injected before the post filter so the horn
            // colours them the same way it colours the programme. `bed` is the
            // channel being open — 1 whenever PTT is off.
            s += key.bed * hissGain * (rng.nextFloat() * 2.0f - 1.0f);

            humPhase += humInc;
            if (humPhase > juce::MathConstants<float>::twoPi)
                humPhase -= juce::MathConstants<float>::twoPi;

            s += key.bed * humGain * (0.35f * std::sin (humPhase)
                                    + 0.60f * std::sin (humPhase * 3.0f)
                                    + 0.45f * std::sin (humPhase * 5.0f)
                                    + 0.25f * std::sin (humPhase * 7.0f));

            // the relay itself: after the gate, before the post filter
            s += key.inject;

            s = postHpf.processSample (s);
            s = postLpf.processSample (s);

            // DC block
            const float y = s - dcX1 + 0.9985f * dcY1;
            dcX1 = s; dcY1 = y;

            x[i] = y;
        }
    }

    const EraProfile& getProfile() const noexcept { return profile; }

private:
    static inline float saturate (float x, float asym, float hardness) noexcept
    {
        const float bias = asym * 0.35f;
        const float soft = std::tanh (x + bias) - std::tanh (bias);
        const float hard = juce::jlimit (-0.85f, 0.85f, soft * (1.0f + hardness * 2.2f));
        return juce::jmap (hardness, soft, 0.5f * (soft + hard));
    }

    using Filter = juce::dsp::IIR::Filter<float>;
    using Delay  = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    Filter  hpf, lpf, pk1, pk2, pk3, postHpf, postLpf, howlPk;
    Delay   wobbleDelay { 4096 }, howlDelay { 8192 };
    PttGate ptt;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::Random rng;

    EraProfile profile = Eras::modern;
    double sr = 44100.0;
    float lastVintage = -1.0f;

    float drive = 1.0f, howlAmt = 0.0f;
    float eraDrive = 1.0f, eraTrim = 1.0f;
    float hissGain = 0.0f, humGain = 0.0f;
    float wobInc = 0.0f, humInc = 0.0f, compSlope = 0.0f;
    float attCoef = 0.0f, relCoef = 0.0f;
    float env = 0.0f, howlState = 0.0f;
    float wobPhase = 0.0f, humPhase = 0.0f;
    float dcX1 = 0.0f, dcY1 = 0.0f;
    float howlDelaySamples = 400.0f;
};
