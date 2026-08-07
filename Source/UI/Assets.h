#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

/*  ============================================================================
    Assets

    Two ways to replace the placeholder artwork:

    1. BUILD TIME — overwrite the files in Resources/ (keep the same filenames)
       and rebuild. They are compiled in via juce_add_binary_data.

    2. RUN TIME — drop replacement SVGs into

           ~/Documents/TannoyBox/Skin/          (macOS / Linux)
           %USERPROFILE%\Documents\TannoyBox\Skin\   (Windows)

       using the same filenames. Anything found there wins, so you can iterate
       on the artwork without touching a compiler. Delete a file to fall back
       to the built-in version.

    Filenames and their design-space sizes:
       background.svg      640 x 480
       knob_face.svg       196 x 196   (drawn centred, does not rotate)
       knob_pointer.svg    196 x 196   (rotates about the centre of the frame)
       knob_small_face.svg  64 x  64
       logo.svg            240 x  52
       nameplate.svg       116 x 180
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

    inline std::unique_ptr<juce::Drawable> load (const juce::String& fileName,
                                                 const char* binaryData,
                                                 int binarySize)
    {
        const auto f = skinFolder().getChildFile (fileName);

        if (f.existsAsFile())
            if (auto d = juce::Drawable::createFromSVGFile (f))
                return d;

        if (binaryData != nullptr)
            return juce::Drawable::createFromImageData (binaryData, (size_t) binarySize);

        return {};
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
