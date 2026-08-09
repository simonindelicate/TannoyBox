#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

/*  ============================================================================
    Assets

    Everything is referenced by STEM ONLY — "background", not "background.svg".
    The loader resolves the stem in this order and takes the first hit:

        1. <skin folder>/<stem>.png        runtime override, no rebuild
        2. <skin folder>/<stem>.svg
        3. built-in  <stem>_png            compiled in from Resources/
        4. built-in  <stem>_svg

    PNG wins over SVG at every level, so you can migrate one element at a time:
    drop background.png into Resources/, rebuild, and the SVG is simply no
    longer reached. Nothing needs deleting and nothing needs editing here.

    Skin folder:
        ~/Documents/TannoyBox/Skin/                  (macOS / Linux)
        %USERPROFILE%\Documents\TannoyBox\Skin\      (Windows)

    ---------------------------------------------------------------- asset list

    background      640 x 480 design units. Author PNGs at 2x (1280 x 960) —
                    the panel is downscaled to the window, and downscaling looks
                    fine where upscaling does not. All the static lettering
                    lives here, including the legends under the two switches.
    logo            240 x 52     drawn at 26,16
    nameplate       116 x 180    drawn at 262,124; keep y 26..116 clear
    knob_large      180 x 180 when square — see below
    knob_small      64 x 64 when square — used for controls under 100 px
    knob_pointer    only used when the knob art is a single static frame

    The PTT and CHIME switches are drawn in code rather than from assets, at
    436,18 and 528,18, both 80 x 38. They are lit from their parameters, so an
    asset would have to come in at least two states; the background artwork
    carries their legends and the plugin draws the cap and lamp on top.

    ------------------------------------------------------------- knob strategy

    Two ways to draw a knob, chosen automatically from the image proportions:

    FILMSTRIP (what you want for PNG). One tall image containing N square frames
    stacked vertically, frame 0 fully anticlockwise, frame N-1 fully clockwise.
    The frame count is inferred as height / width, so a 200 x 25600 image is
    128 frames of 200 x 200 and there is nothing to configure. Shading,
    highlights and cast shadows all rotate with the knob, which is why every
    commercial plugin does it this way. Tools/make_filmstrip.py will bake a
    strip from a single frame if your renderer doesn't produce one.

    SINGLE FRAME. A square image is treated as a static face, and knob_pointer
    is drawn on top and rotated about the centre. Cheaper to author, but the
    lighting won't move and it tends to look flat under a PNG.

    In filmstrip mode the code stops drawing its own value arc, on the
    assumption that your artwork carries its own indication.
    ============================================================================
*/

namespace Assets
{
    inline juce::File skinFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("TannoyBox")
                   .getChildFile ("Skin");
    }

    /** Resolves a stem to raw bytes, preferring PNG and preferring the skin
        folder. Returns an empty block if nothing matches. */
    inline juce::MemoryBlock loadBytes (const juce::String& stem)
    {
        for (auto* ext : { ".png", ".svg" })
        {
            const auto f = skinFolder().getChildFile (stem + ext);

            if (f.existsAsFile())
            {
                juce::MemoryBlock mb;

                if (f.loadFileAsData (mb) && mb.getSize() > 0)
                    return mb;
            }
        }

        for (auto* suffix : { "_png", "_svg" })
        {
            int size = 0;
            const auto name = stem + suffix;

            if (auto* data = BinaryData::getNamedResource (name.toRawUTF8(), size))
                if (size > 0)
                    return juce::MemoryBlock (data, (size_t) size);
        }

        return {};
    }

    /** Vector or bitmap, wrapped as a Drawable. Use for anything that is simply
        painted into a rectangle. */
    inline std::unique_ptr<juce::Drawable> loadDrawable (const juce::String& stem)
    {
        const auto mb = loadBytes (stem);

        if (mb.getSize() == 0)
            return {};

        return juce::Drawable::createFromImageData (mb.getData(), mb.getSize());
    }

    /** Bitmap only — returns an invalid Image for an SVG. Use where you need
        pixels, i.e. filmstrips. */
    inline juce::Image loadImage (const juce::String& stem)
    {
        const auto mb = loadBytes (stem);

        if (mb.getSize() == 0)
            return {};

        return juce::ImageFileFormat::loadFrom (mb.getData(), mb.getSize());
    }
}

namespace Palette
{
    const juce::Colour panel { 0xff2e332e };   // hammertone grey-green
    const juce::Colour ink   { 0xff171a17 };   // stencil black
    const juce::Colour cream { 0xffd8cfb8 };   // painted lettering
    const juce::Colour rust  { 0xffb24a28 };   // oxblood accent
    const juce::Colour moss  { 0xff7e8a7b };   // engraved highlight
}
