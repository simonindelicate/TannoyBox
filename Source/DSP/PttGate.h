#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

/*  ============================================================================
    PttGate

    Silence between announcements is not silent in real life, and it does not
    start or stop cleanly either. Somebody presses a key, a relay closes, the
    amplifier's noise floor arrives, then the voice; at the end the voice stops
    but the channel stays open for a moment before the relay drops out with a
    thump and takes the hiss with it.

    This models the key, not the speech. It watches the programme and drives
    three things per sample:

        gate    multiplies the programme
        bed     multiplies the horn's hiss and hum, so the noise floor only
                exists while the channel is open
        inject  the key click and the release thump, added downstream of the
                gate so they are not silenced by it

    One honest limitation: there is no lookahead, so the key click lands with
    the start of the phrase rather than a beat before it. The trailing side —
    hold, thump, slow decay of the bed — is where most of the character is
    anyway, and that one is exact.

    Disabling does not hard-bypass. The switch crossfades to gate = 1, bed = 1,
    inject = 0 over 30 ms, because a gate that snaps open is a click.
    ============================================================================
*/

class PttGate
{
public:
    struct Frame
    {
        float gate;     // programme multiplier
        float bed;      // hiss / hum multiplier
        float inject;   // click and thump, added after the gate
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        // The detector release is deliberately much shorter than the hold. Slow
        // it down and the envelope, not the hold counter, decides when the
        // channel drops — which makes the hold time a lie and pushes the thump
        // most of a second past the end of the phrase.
        detAtt = coefFor (0.0015);
        detRel = coefFor (0.0300);

        gateOpenStep  = stepFor (0.008);
        gateCloseStep = stepFor (0.045);
        bedOpenStep   = stepFor (0.012);
        bedCloseStep  = stepFor (0.250);
        bypassStep    = stepFor (0.030);

        holdSamples  = (int) (sr * 0.350);
        closeSamples = (int) (sr * 0.045);
        tailSamples  = (int) (sr * 0.320);

        clickDec = std::exp (-1.0f / (float) (0.0018 * sr));
        thumpDec = std::exp (-1.0f / (float) (0.0700 * sr));
        knockDec = std::exp (-1.0f / (float) (0.0140 * sr));
        thumpInc = juce::MathConstants<float>::twoPi * 190.0f / (float) sr;

        reset();
    }

    void reset()
    {
        state = State::closed;
        det = 0.0f;
        gate = bed = 0.0f;
        quiet = timer = thumpDelay = 0;
        clickAmp = thumpAmp = knockAmp = 0.0f;
        thumpPhase = 0.0f;
        bypass = enabled ? 0.0f : 1.0f;
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    /** Feed the programme, post-saturation. Returns the three multipliers for
        this sample. */
    Frame tick (float in) noexcept
    {
        // ---- envelope -------------------------------------------------------
        const float rect = std::abs (in);
        det += (rect > det ? detAtt : detRel) * (rect - det);

        // ---- key state ------------------------------------------------------
        switch (state)
        {
            case State::closed:
                if (det > openThresh)
                    keyUp();
                break;

            case State::open:
                if (det < closeThresh)
                {
                    if (++quiet >= holdSamples)
                    {
                        state = State::tail;
                        timer = tailSamples;
                        thumpDelay = closeSamples;
                    }
                }
                else
                {
                    quiet = 0;
                }
                break;

            case State::tail:
                // Speaking again before the channel has finished closing simply
                // re-keys it, exactly as leaning on the button again would.
                if (det > openThresh)
                {
                    keyUp();
                }
                else
                {
                    if (thumpDelay > 0 && --thumpDelay == 0)
                    {
                        thumpAmp   = thumpLevel;
                        knockAmp   = knockLevel;
                        thumpPhase = 0.0f;
                    }

                    if (--timer <= 0)
                        state = State::closed;
                }
                break;
        }

        // ---- ramps ----------------------------------------------------------
        const bool wantOpen = (state == State::open);

        gate = wantOpen ? juce::jmin (1.0f, gate + gateOpenStep)
                        : juce::jmax (0.0f, gate - gateCloseStep);

        const bool wantBed = (state != State::closed);

        bed = wantBed ? juce::jmin (1.0f, bed + bedOpenStep)
                      : juce::jmax (0.0f, bed - bedCloseStep);

        // ---- transients ------------------------------------------------------
        float inject = 0.0f;

        if (clickAmp > 1.0e-5f)
        {
            inject += clickAmp * (rng.nextFloat() * 2.0f - 1.0f);
            clickAmp *= clickDec;
        }

        if (thumpAmp > 1.0e-5f)
        {
            thumpPhase += thumpInc;
            inject += thumpAmp * std::sin (thumpPhase);
            thumpAmp *= thumpDec;
        }

        if (knockAmp > 1.0e-5f)
        {
            inject += knockAmp * (rng.nextFloat() * 2.0f - 1.0f);
            knockAmp *= knockDec;
        }

        // ---- switch crossfade -------------------------------------------------
        bypass = enabled ? juce::jmax (0.0f, bypass - bypassStep)
                         : juce::jmin (1.0f, bypass + bypassStep);

        return { gate + (1.0f - gate) * bypass,
                 bed  + (1.0f - bed)  * bypass,
                 inject * (1.0f - bypass) };
    }

private:
    enum class State { closed, open, tail };

    void keyUp() noexcept
    {
        state = State::open;
        quiet = 0;
        thumpDelay = 0;

        if (gate < 0.5f)            // only click on a genuine key-up
            clickAmp = clickLevel;
    }

    float coefFor (double seconds) const noexcept
    {
        return 1.0f - std::exp (-1.0f / (float) (seconds * sr));
    }

    float stepFor (double seconds) const noexcept
    {
        return 1.0f / (float) juce::jmax (1.0, seconds * sr);
    }

    // Thresholds are absolute, measured on the post-saturation programme, and
    // ~10 dB apart so a decaying word does not chatter the gate.
    static constexpr float openThresh  = 0.0040f;   // about -48 dBFS
    static constexpr float closeThresh = 0.0013f;   // about -58 dBFS

    // Relay noise, set well under the programme it interrupts. These are the
    // levels going into the post band-limit, which takes a good deal of the
    // thump away again on the older profiles — as a real horn would. Measured
    // through the 1950s profile they land about 10 dB below speech, which is
    // audible as punctuation without becoming the loudest thing in the track.
    static constexpr float clickLevel = 0.055f;
    static constexpr float thumpLevel = 0.055f;
    static constexpr float knockLevel = 0.035f;

    juce::Random rng;
    double sr = 44100.0;

    State state = State::closed;
    bool  enabled = false;

    float det = 0.0f, detAtt = 0.0f, detRel = 0.0f;
    float gate = 0.0f, bed = 0.0f, bypass = 1.0f;
    float gateOpenStep = 0.0f, gateCloseStep = 0.0f;
    float bedOpenStep = 0.0f, bedCloseStep = 0.0f, bypassStep = 0.0f;

    int quiet = 0, timer = 0, thumpDelay = 0;
    int holdSamples = 0, closeSamples = 0, tailSamples = 0;

    float clickAmp = 0.0f, clickDec = 0.0f;
    float thumpAmp = 0.0f, thumpDec = 0.0f, thumpPhase = 0.0f, thumpInc = 0.0f;
    float knockAmp = 0.0f, knockDec = 0.0f;
};
