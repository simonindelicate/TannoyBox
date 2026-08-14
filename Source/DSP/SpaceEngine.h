#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/*  ============================================================================
    SpaceEngine

    Mono in, stereo out. This is the room the horn is shouting into.

    SIZE moves one physical variable — the distance to the far wall — and
    everything else follows from it:

        size 0.00   corridor        5 m     tight slap, 0.35 s tail, bright
        size 0.35   station hall   40 m     distinct doubling, 1.2 s
        size 0.70   terminal       80 m     separate horns audible, 2.6 s
        size 1.00   stadium       115 m     0.35 s predelay, 5.6 s, very dark

    Three layers:
      1. predelay      — time of flight to the nearest horn
      2. discrete taps — the OTHER horns, further down the concourse, panned
                         and progressively duller
      3. FDN tail      — 8 delay lines, Hadamard feedback, damping in the loop

    The taps are what make it read as a public address system rather than a
    reverb plugin: a PA system is always several speakers at different distances.

    ROOM decides how much of the output is room and how much is the horn's own
    direct arrival, with unity on both at the centre:

        0.0   direct arrival only — no taps, no tail, no room at all
        0.5   both at full, which is the balance the physical model gives
        1.0   taps and tail only, nothing direct — a send/return delay

    Below centre it is a level control on the room; above centre it fades the
    direct arrival out from under it. The predelay stays on the direct arrival
    at every position, because time of flight belongs to SIZE, not to this —
    a horn 115 m away really is 100 ms late whether or not you want its room.
    ============================================================================
*/

class SpaceEngine
{
public:
    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;

        preLine.setSize ((int) (sampleRate * 2.0));

        static const float apMs[4] { 5.3f, 8.7f, 12.1f, 17.9f };
        for (int i = 0; i < 4; ++i)
            diffuser[i].setSize ((int) (sampleRate * apMs[i] * 0.001f) + 2);

        for (int i = 0; i < 8; ++i)
            line[i].setSize ((int) (sampleRate * 0.45) + 64);

        blendCoef = onePoleCoef (15.0f);

        lastSize = -1.0f;
        setSize (0.35f);

        // Defaults stand in until the host tells us where ROOM actually is; the
        // first call after prepare snaps rather than glides, so opening a
        // session set to ROOM ONLY does not leak 50 ms of direct horn.
        setRoomAmount (0.5f);
        blendPrimed = false;

        reset();
    }

    void reset()
    {
        preLine.clear();
        for (auto& d : diffuser) d.clear();
        for (auto& l : line)     l.clear();
        for (auto& v : damp)     v = 0.0f;
        for (auto& v : tapLp)    v = 0.0f;
        airL = airR = 0.0f;
        modPhase[0] = 0.0f; modPhase[1] = 1.7f;
    }

    void setSize (float size01)
    {
        if (std::abs (size01 - lastSize) < 1.0e-4f)
            return;

        lastSize = size01;
        const float s = juce::jlimit (0.0f, 1.0f, size01);

        // --- geometry --------------------------------------------------------
        const float metres   = 5.0f + 110.0f * s;
        const float flightMs = metres / 0.343f;               // ms to nearest horn

        preSamples = (float) (sr * 0.001 * (4.0 + 0.30 * flightMs));

        static const float ratio[4] { 1.0f,  1.72f, 2.45f, 3.31f };
        static const float gain [4] { 0.55f, 0.40f, 0.27f, 0.17f };
        static const float pan  [4] { 0.28f, 0.78f, 0.42f, 0.88f };

        for (int k = 0; k < 4; ++k)
        {
            const float ms = flightMs * ratio[k] * 0.55f + 11.0f;
            tapSamples[k]  = juce::jmin ((float) (sr * 1.8), (float) (sr * 0.001 * ms) + preSamples);
            tapGain[k]     = gain[k] * (0.30f + 0.70f * s);
            tapPanL[k]     = std::cos (pan[k] * juce::MathConstants<float>::halfPi);
            tapPanR[k]     = std::sin (pan[k] * juce::MathConstants<float>::halfPi);

            // further horns are duller: air eats the top end
            const float cut = 9000.0f * std::pow (0.55f, (float) k) * (1.0f - 0.55f * s) + 900.0f;
            tapCoef[k] = onePoleCoef (cut);
        }

        // --- tail ------------------------------------------------------------
        const float rt60  = 0.35f * std::pow (16.0f, s);
        const float scale = 0.40f + 2.05f * s;

        static const float baseMs[8] { 23.4f, 31.7f, 41.3f, 53.9f, 67.1f, 79.3f, 89.7f, 103.1f };

        for (int i = 0; i < 8; ++i)
        {
            lineSamples[i] = (float) (sr * 0.001 * baseMs[i] * scale);
            const float sec = lineSamples[i] / (float) sr;
            fb[i] = std::pow (10.0f, -3.0f * sec / rt60);
        }

        dampCoef = onePoleCoef (9000.0f * std::pow (0.22f, s) + 900.0f);
        airCoef  = onePoleCoef (14000.0f * std::pow (0.17f, s) + 1400.0f);
        tailGain = 0.45f + 0.35f * s;
        modDepth = (float) (sr * 0.001 * (0.4f + 1.6f * s));
        modInc[0] = juce::MathConstants<float>::twoPi * 0.31f / (float) sr;
        modInc[1] = juce::MathConstants<float>::twoPi * 0.47f / (float) sr;
    }

    /** 0 = direct only, 0.5 = the model's own balance, 1 = room only. Smoothed
        per sample inside process(), so this is safe to call once per block. */
    void setRoomAmount (float room01) noexcept
    {
        const float r = juce::jlimit (0.0f, 1.0f, room01);

        roomTarget   = juce::jmin (1.0f, r * 2.0f);
        directTarget = juce::jmin (1.0f, (1.0f - r) * 2.0f);

        if (! blendPrimed)
        {
            roomG   = roomTarget;
            directG = directTarget;
            blendPrimed = true;
        }
    }

    void process (const float* in, float* outL, float* outR, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            preLine.push (in[i]);
            const float pre = preLine.read (preSamples);

            // --- discrete horns ---------------------------------------------
            float tl = 0.0f, tr = 0.0f, tapSum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                const float raw = preLine.read (tapSamples[k]);
                tapLp[k] += tapCoef[k] * (raw - tapLp[k]);
                const float t = tapLp[k] * tapGain[k];
                tl += t * tapPanL[k];
                tr += t * tapPanR[k];
                tapSum += t;
            }

            // --- diffusion ---------------------------------------------------
            float d = pre * 0.75f + tapSum * 0.22f;
            for (auto& ap : diffuser)
                d = ap.process (d);

            // --- FDN ----------------------------------------------------------
            modPhase[0] += modInc[0];
            modPhase[1] += modInc[1];

            float v[8];
            for (int j = 0; j < 8; ++j)
            {
                float dl = lineSamples[j];
                if (j == 2) dl += modDepth * std::sin (modPhase[0]);
                if (j == 5) dl += modDepth * std::sin (modPhase[1]);

                v[j] = line[j].read (dl);
                damp[j] += dampCoef * (v[j] - damp[j]);
                v[j] = damp[j];
            }

            const float wetL = v[0] + v[2] - v[4] + v[6];
            const float wetR = v[1] - v[3] + v[5] + v[7];

            float f[8];
            for (int j = 0; j < 8; ++j)
                f[j] = v[j] * fb[j];

            hadamard8 (f);

            for (int j = 0; j < 8; ++j)
                line[j].push (d * 0.35f + f[j]);

            // --- room / direct balance ------------------------------------------
            roomG   += blendCoef * (roomTarget   - roomG);
            directG += blendCoef * (directTarget - directG);

            // --- air absorption on the whole wet signal ------------------------
            const float direct = pre * 0.55f * directG;
            const float mixL   = (tl + wetL * tailGain * 0.35f) * roomG + direct;
            const float mixR   = (tr + wetR * tailGain * 0.35f) * roomG + direct;

            airL += airCoef * (mixL - airL);
            airR += airCoef * (mixR - airR);

            outL[i] = airL;
            outR[i] = airR;
        }
    }

private:
    float onePoleCoef (float hz) const noexcept
    {
        return juce::jlimit (0.002f, 1.0f,
            1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr));
    }

    static void hadamard8 (float* v) noexcept
    {
        for (int step = 1; step < 8; step <<= 1)
            for (int i = 0; i < 8; i += step << 1)
                for (int j = i; j < i + step; ++j)
                {
                    const float a = v[j], b = v[j + step];
                    v[j] = a + b;
                    v[j + step] = a - b;
                }

        const float norm = 0.35355339f;   // 1 / sqrt(8)
        for (int i = 0; i < 8; ++i)
            v[i] *= norm;
    }

    struct Line
    {
        std::vector<float> buf;
        int w = 0;

        void setSize (int n)          { buf.assign ((size_t) juce::jmax (4, n), 0.0f); w = 0; }
        void clear()                  { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float v) noexcept  { buf[(size_t) w] = v; if (++w >= (int) buf.size()) w = 0; }

        float read (float delaySamples) const noexcept
        {
            const int    sz = (int) buf.size();
            const float  d  = juce::jlimit (1.0f, (float) sz - 2.0f, delaySamples);
            const int    di = (int) d;
            const float  fr = d - (float) di;

            int i0 = w - 1 - di;  while (i0 < 0) i0 += sz;
            int i1 = i0 - 1;      if (i1 < 0) i1 += sz;

            return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
        }
    };

    struct Allpass
    {
        std::vector<float> buf;
        int idx = 0;
        float g = 0.62f;

        void setSize (int n) { buf.assign ((size_t) juce::jmax (4, n), 0.0f); idx = 0; }
        void clear()         { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; }

        float process (float x) noexcept
        {
            const float b = buf[(size_t) idx];
            const float y = -x + b;
            buf[(size_t) idx] = x + b * g;
            if (++idx >= (int) buf.size()) idx = 0;
            return y;
        }
    };

    Line    preLine, line[8];
    Allpass diffuser[4];

    double sr = 44100.0;
    float  lastSize = -1.0f;

    float preSamples = 100.0f;
    float tapSamples[4] {}, tapGain[4] {}, tapPanL[4] {}, tapPanR[4] {}, tapCoef[4] {}, tapLp[4] {};
    float lineSamples[8] {}, fb[8] {}, damp[8] {};
    float dampCoef = 0.5f, airCoef = 0.5f, tailGain = 0.6f;
    float modDepth = 0.0f, modPhase[2] {}, modInc[2] {};
    float airL = 0.0f, airR = 0.0f;
    float roomTarget = 1.0f, directTarget = 1.0f;
    float roomG = 1.0f, directG = 1.0f, blendCoef = 0.002f;
    bool  blendPrimed = false;
};
