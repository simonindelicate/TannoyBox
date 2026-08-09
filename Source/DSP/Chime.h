#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

/*  ============================================================================
    Chime

    Mono. The four-note tubular ding that precedes an announcement.

    It is generated here and summed into the horn's input, not mixed in later,
    which matters for two reasons. The horn's passband and saturation are what
    make a chime sound like it is coming out of a station loudspeaker rather
    than out of an orchestra — a 415 Hz fundamental barely survives the 1950s
    profile's 350 Hz high-pass, so what you actually hear is the second and
    third partials, which is exactly what a real one sounds like. And because
    the PTT gate detects on the post-saturation signal, a chime keys the gate
    open by itself: hiss bed, then ding, then speech.

    Each note is a struck bar: five inharmonic partials at the classical
    tubular-bell ratios, each with its own decay, plus a broadband strike
    transient. Notes overlap, so there is one voice per note in the phrase.

    The phrase is the Westminster quarter — G#, F#, E, B descending — because
    it is the one every British listener has heard through a horn.
    ============================================================================
*/

class Chime
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        strikeSpacing = (int) (sr * 0.400);
        reset();
    }

    void reset()
    {
        for (auto& v : voice)
            v.clear();

        next = kNotes;          // nothing pending
        countdown = 0;
        nextVoice = 0;
    }

    /** Starts the phrase from the top. Safe to call mid-ring — the voices
        already sounding are left to decay under the new ones. */
    void trigger() noexcept
    {
        next = 0;
        countdown = 0;
    }

    bool isActive() const noexcept
    {
        if (next < kNotes)
            return true;

        for (auto& v : voice)
            if (v.active)
                return true;

        return false;
    }

    /** Adds into a mono block. Does nothing at all when silent. */
    void process (float* x, int n) noexcept
    {
        if (! isActive())
            return;

        for (int i = 0; i < n; ++i)
        {
            if (next < kNotes && --countdown <= 0)
            {
                strike (next);
                ++next;
                countdown = strikeSpacing;
            }

            float s = 0.0f;

            for (auto& v : voice)
                s += v.tick (rng);

            x[i] += s;
        }
    }

private:
    static constexpr int kNotes    = 4;
    static constexpr int kVoices   = 4;
    static constexpr int kPartials = 5;

    // Westminster quarter, in semitones from the first note.
    static constexpr int   step[kNotes]  { 0, -2, -4, -9 };
    static constexpr float level[kNotes] { 1.00f, 0.92f, 0.88f, 1.00f };

    // Rayleigh's tubular bell: the partials are nowhere near harmonic, and the
    // pitch you hear is not the lowest one. Higher partials die first.
    //
    // The decays are short for a bell — a real orchestral tube rings for ten
    // seconds or more. These are chime bars in a box on a wall, and four of
    // them overlapping for that long through a horn is porridge.
    static constexpr float ratio[kPartials] { 1.00f, 2.00f, 2.76f, 5.40f, 8.93f };
    static constexpr float weight[kPartials] { 1.00f, 0.62f, 0.45f, 0.24f, 0.12f };
    static constexpr float decaySec[kPartials] { 0.70f, 0.56f, 0.42f, 0.28f, 0.18f };

    static constexpr float rootHz = 415.30f;   // G#4

    // Five partials plus a strike sum to about 2.6x this, and up to four notes
    // overlap, so the phrase peaks near 0.5 going into the saturator.
    static constexpr float voiceGain = 0.16f;

    // -80 dB of a voice: below anything the horn's noise floor would show.
    static constexpr float silence = 3.0e-4f;

    struct Voice
    {
        float phase[kPartials] {}, inc[kPartials] {}, amp[kPartials] {}, dec[kPartials] {};
        float strikeAmp = 0.0f, strikeDec = 0.0f;
        bool  active = false;

        void clear()
        {
            for (int p = 0; p < kPartials; ++p)
                phase[p] = inc[p] = amp[p] = dec[p] = 0.0f;

            strikeAmp = strikeDec = 0.0f;
            active = false;
        }

        float tick (juce::Random& rng) noexcept
        {
            if (! active)
                return 0.0f;

            float s = 0.0f;
            float loudest = 0.0f;

            for (int p = 0; p < kPartials; ++p)
            {
                phase[p] += inc[p];
                if (phase[p] > juce::MathConstants<float>::twoPi)
                    phase[p] -= juce::MathConstants<float>::twoPi;

                s += amp[p] * std::sin (phase[p]);
                amp[p] *= dec[p];
                loudest = juce::jmax (loudest, amp[p]);
            }

            // The hammer, not the bar. Short, broadband, shaped by the horn.
            s += strikeAmp * (rng.nextFloat() * 2.0f - 1.0f);
            strikeAmp *= strikeDec;

            if (loudest < silence && strikeAmp < silence)
                active = false;

            return s;
        }
    };

    void strike (int note) noexcept
    {
        auto& v = voice[nextVoice];
        nextVoice = (nextVoice + 1) % kVoices;

        const float hz  = rootHz * std::pow (2.0f, (float) step[note] / 12.0f);
        const float lvl = level[note] * voiceGain;

        for (int p = 0; p < kPartials; ++p)
        {
            // A little phase spread, so the partials do not all arrive together
            // and give the attack a synthetic click of their own.
            v.phase[p] = 0.61f * (float) p;
            v.inc[p]   = juce::MathConstants<float>::twoPi * hz * ratio[p] / (float) sr;
            v.amp[p]   = lvl * weight[p];
            v.dec[p]   = std::exp (-1.0f / (float) (decaySec[p] * sr));
        }

        v.strikeAmp = lvl * 0.55f;
        v.strikeDec = std::exp (-1.0f / (float) (0.006 * sr));
        v.active    = true;
    }

    Voice voice[kVoices];
    juce::Random rng;

    double sr = 44100.0;
    int strikeSpacing = 17640;
    int next = kNotes;
    int countdown = 0;
    int nextVoice = 0;
};
