# TannoyBox

A VST3/AU plugin that puts an audio source through a public address horn and the
room it is hanging in. One job: make a voice sound like it is coming out of a
tannoy. Two large dials, four small ones, nothing else.

---

## Build

Requires CMake 3.22+ and a C++17 compiler. JUCE is fetched automatically if you
haven't put a checkout at `./JUCE`.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Windows (Visual Studio 2022):

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output lands in `build/TannoyBox_artefacts/Release/` and, because
`COPY_PLUGIN_AFTER_BUILD` is on, is also installed to your system plugin folder.
Build the Standalone target first to audition without a host.

Pinned to JUCE 8.0.4. It compiles against JUCE 7 as well — the one
version-sensitive call is font construction, which is already guarded in
`TannoyLookAndFeel::stencil()`.

---

## Signal chain

Mono is not an option, it is the point: the wet path is summed to mono before
the horn, because a PA is one amplifier driving one loudspeaker. Stereo only
comes back at the room stage.

```
input ─┬─────────────────────────────── dry ──────────────────┐
       └─ sum to mono                                         │
            │                                                 │
            ├─ HORN  pre band-limit ─ formant bells ─ wobble   │
            │        ─ [4x oversampled saturation]            │
            │        ─ line-amp limiter ─ howl ─ hiss+hum      │
            │        ─ post band-limit ─ DC block             │
            │                                                 │
            └─ ROOM  predelay ─ 4 panned horn taps            │
                     ─ 4 allpass diffusers                    │
                     ─ 8-line Hadamard FDN ─ air absorption ──┤
                                                              │
                                                     mix ─ output
```

Two details that do most of the convincing work:

**Band-limiting on both sides of the distortion.** The horn only gets fed what
it can reproduce, and the distortion products are then trapped inside the same
passband. Skip the second filter and it sounds like a fuzz pedal.

**Discrete taps before the reverb.** A tannoy is never one speaker. Four
delayed, panned, progressively duller copies arrive before the tail does, and
that comb-flutter is what the ear reads as "station announcement" rather than
"reverb plugin".

---

## Controls

| Control | Range | What it does |
|---|---|---|
| **VINTAGE** | MODERN → 1950s | Morphs continuously through four voicings. Detents at each era; it interpolates between them, so 1965 is a real position. |
| **SIZE** | 5 m → 115 m | One physical variable — distance to the far wall. Predelay, tap spacing, tap level, RT60, damping and air absorption all follow from it. |
| DRIVE | ±12 dB | Trim into the saturator, on top of the era's own drive. |
| HOWL | 0–100% | Feedback resonance on the strongest formant. A limiter inside the loop keeps it musical rather than destructive. Leave at 0 for straight work. |
| MIX | 0–100% | Dry/PA blend. |
| OUTPUT | −24 → +12 dB | Make-up. |

### The four era profiles

| | Passband | Character | Drive | Noise |
|---|---|---|---|---|
| **1950s** | 350 Hz – 2.9 kHz | Hard-honking formant at 1.45 kHz, valve amp being abused, driver sags on transients | +18 dB, heavily asymmetric | Mains buzz and hiss both audible |
| **1960s** | 260 Hz – 4.3 kHz | Mid-forward, warm clip, less honk | +13.5 dB | Present but polite |
| **1970s** | 190 Hz – 6.2 kHz | Solid state, plastic re-entrant horn, splashy top | +9 dB | Low |
| **MODERN** | 95 Hz – 13.5 kHz | Wide, flat, DSP-limited. Deliberately undistorted | +2 dB | None |

Every number lives in `Source/DSP/EraProfiles.h` and nowhere else. To re-voice
an era, edit that file. To reverse the dial so MODERN is fully clockwise, flip
the order of `table[]` in `interpolateEra()`.

---

## Replacing the artwork

Six SVGs in `Resources/`, all obviously placeholder — dashed pink construction
marks live in a `<g id="placeholder-guides">` group in each file, so deleting
that one group cleans a file up.

| File | Design size | Notes |
|---|---|---|
| `background.svg` | 640 × 480 | The whole panel, **including all static lettering**: knob names, scale legends, captions. |
| `knob_face.svg` | 196 × 196 | Drawn centred; never rotates. |
| `knob_pointer.svg` | 196 × 196 | Rotates about the centre of its own frame — must point straight up at rest. |
| `knob_small_face.svg` | 64 × 64 | Used automatically for controls under 100 px. |
| `logo.svg` | 240 × 52 | Brand mark, drawn at 26,20. |
| `nameplate.svg` | 116 × 180 | Readout window. Keep y 26–116 clear; the plugin writes live text there. |

Static text is on the panel rather than in the code so that replacing
`background.svg` re-types the whole instrument in one move.

**Two ways to swap them:**

1. Overwrite the files and rebuild.
2. Drop replacements into `~/Documents/TannoyBox/Skin/` (or
   `%USERPROFILE%\Documents\TannoyBox\Skin\`) using the same filenames. Those
   win at load time, so you can iterate without a compiler. Delete a file to
   fall back.

**Preview without building anything:** open `Tools/panel-preview.html`. It
renders the panel with working dials, and you can drag a replacement `.svg`
onto the page to see it in place immediately. Re-run
`python3 Tools/make_preview.py` to bake changes back in.

If you move a control, the coordinates live in two places that must agree:
`Layout::` at the top of `Source/PluginEditor.cpp`, and the matching constants
in `Tools/make_background.py`.

---

## Worth considering next

Deliberately left out to keep the brief honest, in rough order of how much they
would add:

- **Impulse response slot.** A convolver in place of the horn's EQ section,
  fed by a real horn IR, with VINTAGE still driving the distortion and noise.
  This is the one change that would take it from convincing to indistinguishable.
- **Chime.** A two- or four-note tubular ding on a trigger, through the same
  horn path. The single most recognisable part of the sound and it costs almost
  nothing.
- **Ducking sidechain.** Announcements over music, with the music pulled down
  by the horn's own envelope.
- **PTT gate.** Key click, a beat of hum and hiss on either side of the phrase,
  release thump. Silence between announcements is not silent in real life.
- **Handheld / horn / column** speaker-type switch — a second axis to VINTAGE
  rather than more of the same one.
- **Era-linked convolution of the taps** so distant horns are not just filtered
  copies but separately voiced boxes.
