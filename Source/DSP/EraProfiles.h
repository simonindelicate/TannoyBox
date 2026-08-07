#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>

/*  ============================================================================
    EraProfiles.h

    Every tonal decision in the plugin lives here. If you want to re-voice an
    era, change numbers in this file and nothing else.

    The VINTAGE dial runs fully anticlockwise = MODERN, fully clockwise = 1950s,
    i.e. turning it up adds age. The four profiles are anchor points at
    0.0 / 0.333 / 0.667 / 1.0 and the dial morphs continuously between them
    (frequencies interpolate logarithmically, everything else linearly).

    To reverse the dial direction, flip the order of `table[]` in
    interpolateEra().
    ============================================================================
*/

struct Formant
{
    float hz;
    float q;
    float gainDb;
};

struct EraProfile
{
    const char* name;

    // Horn passband. Applied twice (once before the nonlinearity, once after),
    // so the effective slope is 4th order. High Q = ringy, cheap-driver edges.
    float hpfHz,  hpfQ;
    float lpfHz,  lpfQ;

    // The honk. Three peaking bells that give the horn its formant signature.
    Formant f1, f2, f3;

    // Amplifier / driver nonlinearity.
    float driveDb;        // gain into the saturator
    float asymmetry;      // 0 = symmetric (odd harmonics), 1 = heavy even order
    float clipHardness;   // 0 = soft tanh, 1 = cone breakup / hard knee

    // Line amp limiting.
    float compRatio;      // 1 = off
    float compThreshDb;

    // Electro-mechanical noise floor.
    float hissDb;         // broadband, injected pre post-EQ so the horn shapes it
    float humDb;          // mains buzz (50 Hz + odd harmonics)

    // Slow instability: sagging amp rails and a driver that isn't quite pinned.
    float wobbleDepth;    // 0..1
    float wobbleRateHz;
};

namespace Eras
{
    // ------------------------------------------------------------------ MODERN
    // Clean 100 V line, compression driver, DSP-limited. Wide, flat, polite.
    static const EraProfile modern
    {
        "MODERN",
        95.0f,  0.72f,
        13500.0f, 0.72f,
        {  900.0f, 1.1f,  0.5f },
        { 2400.0f, 1.4f,  1.5f },
        { 6500.0f, 1.2f, -1.0f },
        2.0f,   0.05f, 0.05f,
        2.0f,  -10.0f,
        -96.0f, -110.0f,
        0.0f,   0.30f
    };

    // ------------------------------------------------------------------- 1970s
    // Solid state, plastic re-entrant horns. Brighter, splashier, still gritty.
    static const EraProfile seventies
    {
        "1970s",
        190.0f, 0.95f,
        6200.0f, 1.10f,
        {  850.0f, 1.8f,  3.0f },
        { 1900.0f, 2.6f,  5.0f },
        { 3800.0f, 3.0f,  4.0f },
        9.0f,   0.22f, 0.35f,
        3.5f,  -16.0f,
        -78.0f, -70.0f,
        0.010f, 0.45f
    };

    // ------------------------------------------------------------------- 1960s
    // Valve amp on its way out, pressed-steel horn. Mid-forward, warm clip.
    static const EraProfile sixties
    {
        "1960s",
        260.0f, 1.15f,
        4300.0f, 1.35f,
        {  820.0f, 2.0f,  4.0f },
        { 1650.0f, 3.1f,  6.5f },
        { 3100.0f, 3.8f,  4.5f },
        13.5f,  0.38f, 0.60f,
        4.5f,  -19.0f,
        -72.0f, -64.0f,
        0.020f, 0.55f
    };

    // ------------------------------------------------------------------- 1950s
    // Tannoy proper: narrow band, honking formants, hard-worked valve amp,
    // audible mains and a driver that sags on transients.
    static const EraProfile fifties
    {
        "1950s",
        350.0f, 1.35f,
        2900.0f, 1.60f,
        {  780.0f, 2.4f,  4.5f },
        { 1450.0f, 3.7f,  8.5f },
        { 2450.0f, 4.6f,  6.0f },
        18.0f,  0.55f, 0.85f,
        6.0f,  -22.0f,
        -66.0f, -58.0f,
        0.032f, 0.62f
    };
}

// ---------------------------------------------------------------- interpolation

inline float eraLerp (float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

inline float eraLerpHz (float a, float b, float t) noexcept
{
    return std::exp (eraLerp (std::log (a), std::log (b), t));
}

inline Formant eraLerp (const Formant& a, const Formant& b, float t) noexcept
{
    return { eraLerpHz (a.hz, b.hz, t), eraLerp (a.q, b.q, t), eraLerp (a.gainDb, b.gainDb, t) };
}

/** Morphs between the four anchors. 0 = MODERN, 1 = 1950s. */
inline EraProfile interpolateEra (float x) noexcept
{
    static const EraProfile* table[4]
    {
        &Eras::modern, &Eras::seventies, &Eras::sixties, &Eras::fifties
    };

    x = std::clamp (x, 0.0f, 1.0f) * 3.0f;
    const int  i = std::min (2, (int) x);
    const float t = x - (float) i;

    const EraProfile& a = *table[i];
    const EraProfile& b = *table[i + 1];

    EraProfile p { t < 0.5f ? a.name : b.name,
        eraLerpHz (a.hpfHz, b.hpfHz, t), eraLerp (a.hpfQ, b.hpfQ, t),
        eraLerpHz (a.lpfHz, b.lpfHz, t), eraLerp (a.lpfQ, b.lpfQ, t),
        eraLerp (a.f1, b.f1, t), eraLerp (a.f2, b.f2, t), eraLerp (a.f3, b.f3, t),
        eraLerp (a.driveDb,      b.driveDb,      t),
        eraLerp (a.asymmetry,    b.asymmetry,    t),
        eraLerp (a.clipHardness, b.clipHardness, t),
        eraLerp (a.compRatio,    b.compRatio,    t),
        eraLerp (a.compThreshDb, b.compThreshDb, t),
        eraLerp (a.hissDb,       b.hissDb,       t),
        eraLerp (a.humDb,        b.humDb,        t),
        eraLerp (a.wobbleDepth,  b.wobbleDepth,  t),
        eraLerp (a.wobbleRateHz, b.wobbleRateHz, t) };

    return p;
}

/** Display string for the readout: shows the morph when between anchors. */
inline juce::String eraDisplayName (float x)
{
    static const char* names[4] { "MODERN", "1970s", "1960s", "1950s" };

    const float s = std::clamp (x, 0.0f, 1.0f) * 3.0f;
    const int   i = std::min (2, (int) s);
    const float t = s - (float) i;

    if (t < 0.06f) return names[i];
    if (t > 0.94f) return names[i + 1];

    return juce::String (names[i]) + " > " + names[i + 1];
}
