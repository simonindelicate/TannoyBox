# TannoyBox — working notes

A JUCE VST3/AU plugin that runs audio through a public address horn and the room
it hangs in. The brief is "one job done perfectly": two large dials, four small
ones, nothing else. Resist feature suggestions that widen the scope; there is a
list at the bottom of README.md of things deliberately left out.

## Build

```
cmake -B build -G "Visual Studio 17 2022" -A x64      # Windows, once
cmake --build build --config Release
```

The configure step fetches JUCE 8.0.4 into `build/` and needs a network. It only
needs re-running when files are added or removed. On Windows the DAW must be
closed before building, because the post-build step overwrites the installed
VST3 and Windows locks loaded DLLs.

Debugging is far easier through `build/TannoyBox.sln` with the
**TannoyBox_Standalone** target set as startup project, than through a host.

## Layout of the code

| Path | Contains |
|---|---|
| `Source/DSP/EraProfiles.h` | Every tonal constant for all four eras. Voicing changes happen here and nowhere else. |
| `Source/DSP/HornBody.h` | Band-limiting, saturation, limiter, howl feedback, hiss and hum. Mono. |
| `Source/DSP/SpaceEngine.h` | Predelay, four panned horn taps, allpass diffusion, 8-line Hadamard FDN, air absorption. Mono in, stereo out. |
| `Source/PluginProcessor.cpp` | Parameter definitions, mono summing, block plumbing. |
| `Source/PluginEditor.cpp` | `Layout::` at the top is the single source of control geometry. |
| `Source/UI/Assets.h` | Asset resolution by stem: skin folder before built-in, PNG before SVG. |
| `Source/UI/TannoyLookAndFeel.*` | Filmstrip or face+pointer knob drawing. |
| `Tools/*.py` | Artwork generators. Not part of the build. |

## Things that will bite

**Geometry is duplicated.** `Layout::` in `PluginEditor.cpp` positions the
controls; the background artwork has the legends and control names baked in at
matching coordinates. Nothing enforces agreement. Move a control and the
lettering stays put. If the background is still the generated SVG, update
`Tools/make_background.py` in the same commit and re-run it.

**The plugin identity codes are frozen.** `PLUGIN_CODE Tnby` and
`PLUGIN_MANUFACTURER_CODE Indl` in CMakeLists.txt must never change once
anything has been released. Hosts key saved sessions off them.

**Parameter IDs are frozen for the same reason.** The strings in `ParamID` in
`PluginProcessor.h` are what get written into saved state. Adding parameters is
safe; renaming or removing one silently breaks every session that used it. If a
parameter has to go, leave the ID in place and stop reading it.

**No allocation, locking or logging in `processBlock`.** IIR coefficients are
deliberately rebuilt only when a dial actually moves, guarded by an epsilon
check in `HornBody::setParameters`. Keep that pattern.

**The `SpaceEngine` buffers are sized in `prepare`.** `SIZE` scales delay times
within already-allocated lines; it must never ask for a longer delay than the
line holds. `Line::read` clamps, but silently, so a geometry change that
overshoots will just sound wrong rather than crash.

## Conventions

JUCE house style, which is what the existing code uses: four spaces, no tabs,
brace on its own line, a space before the parenthesis in a function call
(`foo (x)`), `juce::` qualified rather than `using namespace juce` at file
scope. Comments explain *why*, not what — the DSP files have block comments at
the top of each class explaining the physical model, and those are worth
keeping current when the model changes.

Commit voicing changes separately from structural ones, with messages describing
what it sounded like rather than what was edited. Twenty small tweaks to
`EraProfiles.h` arrive somewhere good and then nobody remembers which one did it.

## Artwork

Assets are referenced by stem — `background`, not `background.svg`. The loader
prefers PNG over SVG and prefers `~/Documents/TannoyBox/Skin/` over the built-in
copies, so artwork can be iterated without rebuilding, and PNGs supersede the
placeholder SVGs simply by existing. See the comment block at the top of
`Source/UI/Assets.h` for sizes and the filmstrip convention.
