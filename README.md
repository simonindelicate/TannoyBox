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

Assets are referenced by **stem** — `background`, not `background.svg` — and
resolved at runtime in this order, first hit wins:

1. `<skin folder>/<stem>.png`
2. `<skin folder>/<stem>.svg`
3. built-in `<stem>.png` from `Resources/`
4. built-in `<stem>.svg`

PNG beats SVG at every level, so migrating to bitmap artwork means dropping
`background.png` into `Resources/` and rebuilding. The placeholder SVG is then
simply never reached. Nothing needs deleting and no code changes.

The skin folder is `~/Documents/TannoyBox/Skin/` (or
`%USERPROFILE%\Documents\TannoyBox\Skin\`). It does not exist until you make
it. Files there override the built-in copies at load time, so closing and
reopening the plugin window is enough to see a change — no rebuild.

| Stem | Design size | Notes |
|---|---|---|
| `background` | 640 × 480 | The whole panel, **including all static lettering**. Author PNGs at 2× (1280 × 960). |
| `knob_large` | 196 × 196 | Filmstrip or single frame — see below. |
| `knob_small` | 64 × 64 | Used automatically for controls under 100 px. |
| `knob_pointer` | 196 × 196 | Only used when the knob art is a single frame. Must point straight up at rest. |
| `logo` | 240 × 52 | Drawn at 26,20. |
| `nameplate` | 116 × 180 | Readout window. Keep y 26–116 clear; the plugin writes live text there. |

### Knob filmstrips

A knob image that is taller than it is wide is treated as a vertically stacked
sprite sheet, frame 0 fully anticlockwise and the last frame fully clockwise.
The frame count is inferred as height ÷ width, so a 200 × 25600 PNG is 128
frames of 200 × 200 and there is nothing to configure. In filmstrip mode the
code stops drawing its own value arc and index marks, assuming the artwork
carries them.

A square image is treated as a static face instead, with `knob_pointer` rotated
over the top. That is cheaper to author but the lighting does not move with the
control, which looks noticeably flat in bitmap.

`Tools/make_filmstrip.py` will bake a strip by rotating a single frame. It is
the quick version, not the best version — a fixed light source, with only the
knob body rotating, is what makes commercial knobs look solid, and that has to
come from your renderer.

**Preview without building anything:** open `Tools/panel-preview.html`. It
renders the panel with working dials, and you can drag a replacement file onto
the page to see it in place immediately.

If you move a control, the coordinates live in two places that must agree:
`Layout::` at the top of `Source/PluginEditor.cpp`, and the constants in
`Tools/make_background.py`.

---

## Distributing it

### Version discipline

Two things must never change once anything is public: `PLUGIN_CODE` and
`PLUGIN_MANUFACTURER_CODE` in CMakeLists.txt, and the parameter ID strings in
`ParamID`. Hosts identify the plugin by the former and write the latter into
saved sessions. Changing either silently breaks every project anyone has made
with it. Bump `project(TannoyBox VERSION ...)` for each release instead.

### Windows

The build already links the MSVC runtime statically, so there is no Visual C++
Redistributable for users to chase.

The simplest distribution is a zip containing the `TannoyBox.vst3` folder and a
one-paragraph note saying to drop it in `C:\Program Files\Common Files\VST3\`
and rescan. That works, and plenty of small plugin developers do nothing more.

`packaging/installer.iss` is a script for [Inno Setup](https://jrsoftware.org/isinfo.php),
which is free. Build in Release, open the script, press Compile, and you get a
single `.exe` that installs the VST3 and the standalone and registers an
uninstaller. Considerably kinder to a non-technical user.

Unsigned, either route will make SmartScreen say "Windows protected your PC"
and hide the Run button behind **More info**. Signing removes that, but a code
signing certificate now has to live on a hardware token or cloud HSM, which
puts it in the region of £200–400 a year through a reseller. Certum's
open-source developer certificate is substantially cheaper if the project
qualifies. Worth checking current prices — this market moves.

### macOS

Universal binaries are already configured. The problem is Gatekeeper, which is
stricter than SmartScreen: an unsigned, un-notarised plugin downloaded from the
web will be refused outright on Apple Silicon rather than merely warned about.
Users can clear it with `xattr -dr com.apple.quarantine`, but telling people to
run terminal commands is not distribution.

Doing it properly means the Apple Developer Program (\$99/year), a Developer ID
Application certificate, `codesign` on each bundle, a `.pkg` built with
`pkgbuild`/`productbuild`, then `notarytool submit --wait` and `stapler staple`.
It is a day of faff the first time and a two-line script forever after.

### Honest advice

Ship unsigned to begin with, as a zip, with clear instructions and a note that
the warning is expected. If people actually use it, pay for signing then. The
Apple fee is the one worth paying first, because on macOS unsigned is closer to
broken than to inconvenient.

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
