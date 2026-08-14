#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BinaryData.h"

/*  ============================================================================
    PresetManager

    Presets are XML, they hold real values rather than normalised ones, and they
    live in two places resolved in one list:

        bundled     compiled in from Resources/Presets/*.xml
        yours       ~/Documents/Yellowcoat/Presets/*.xml

    That is deliberately the same arrangement as the skin folder, and for the
    same reason: a preset you are still working on is a file you can save,
    audition, edit in a text editor and save again without rebuilding anything.
    When a set of them is finished, the files drop into Resources/Presets/ and
    are bundled — no code changes, because the loader does not care where a
    preset came from once it has been read.

    Two rules the file format exists to enforce:

    Every preset writes every parameter. A preset that only mentions the dials
    it cares about half-applies over whatever was there before and never sounds
    the same twice. Anything genuinely missing from a file falls back to that
    parameter's default, not to its current value, so loading is deterministic
    even if the file is old or hand-mangled.

    CHIME is never stored and always cleared. It is a momentary trigger, and a
    preset that set it high would strike the chime on load.
    ============================================================================
*/

class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        juce::File   file;          // empty for bundled
        int          binaryIndex = -1;
        bool isBundled() const noexcept { return binaryIndex >= 0; }
    };

    explicit PresetManager (juce::AudioProcessor& processorToUse,
                            juce::AudioProcessorValueTreeState& stateToUse)
        : processor (processorToUse), state (stateToUse)
    {
        scanBundled();
        refresh();
    }

    //==============================================================================
    static juce::File folder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Yellowcoat")
                   .getChildFile ("Presets");
    }

    /** Re-reads the user folder. Cheap, and worth doing every time the menu is
        opened so a file dropped in from Explorer shows up without a restart. */
    void refresh()
    {
        userPresets.clear();

        const auto dir = folder();

        if (dir.isDirectory())
        {
            for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
                userPresets.add ({ nameOf (juce::parseXML (f).get(), f.getFileNameWithoutExtension()),
                                   f, -1 });

            std::sort (userPresets.begin(), userPresets.end(),
                       [] (const Entry& a, const Entry& b)
                       { return a.name.compareIgnoreCase (b.name) < 0; });
        }
    }

    const juce::Array<Entry>& bundled() const noexcept { return bundledPresets; }
    const juce::Array<Entry>& user()    const noexcept { return userPresets; }

    juce::String currentName() const  { return current; }
    int  currentIndex() const noexcept { return currentBundled; }

    /** Restores the label after a session reload. Does not touch parameters —
        the session's own values have already done that. */
    void setCurrentName (const juce::String& name)
    {
        current = name;
        currentBundled = -1;

        for (int i = 0; i < bundledPresets.size(); ++i)
            if (bundledPresets.getReference (i).name == name)
                currentBundled = i;

        ++rev;
    }

    /** Bumped on every load so a UI polling on a timer can notice. */
    int  revision() const noexcept { return rev; }

    //==============================================================================
    bool load (const Entry& e)
    {
        std::unique_ptr<juce::XmlElement> xml;

        if (e.isBundled())
        {
            int size = 0;
            if (auto* data = BinaryData::getNamedResource (
                    BinaryData::namedResourceList[e.binaryIndex], size))
                xml = juce::parseXML (juce::String::createStringFromData (data, size));
        }
        else
        {
            xml = juce::parseXML (e.file);
        }

        if (xml == nullptr || ! xml->hasTagName ("YELLOWCOAT_PRESET"))
            return false;

        apply (*xml);

        current = e.name;
        currentBundled = e.isBundled() ? bundledPresets.indexOf (e) : -1;
        ++rev;
        return true;
    }

    bool loadBundled (int index)
    {
        return juce::isPositiveAndBelow (index, bundledPresets.size())
                 && load (bundledPresets.getReference (index));
    }

    /** Writes the current settings to the user folder, creating it if needed.
        Returns the file, or an empty File if it could not be written. */
    juce::File saveAs (const juce::String& presetName)
    {
        const auto trimmed = presetName.trim();

        if (trimmed.isEmpty())
            return {};

        const auto dir = folder();

        if (! dir.isDirectory() && ! dir.createDirectory())
            return {};

        const auto file = dir.getChildFile (juce::File::createLegalFileName (trimmed) + ".xml");

        juce::XmlElement xml ("YELLOWCOAT_PRESET");
        xml.setAttribute ("name", trimmed);

        for (auto* p : processor.getParameters())
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto id = ranged->getParameterID();

                if (id == chimeID)          // momentary, never stored
                    continue;

                auto* e = xml.createNewChildElement ("PARAM");
                e->setAttribute ("id", id);
                e->setAttribute ("value", ranged->convertFrom0to1 (ranged->getValue()));
            }
        }

        if (! xml.writeTo (file))
            return {};

        refresh();
        current = trimmed;
        currentBundled = -1;
        ++rev;
        return file;
    }

private:
    /** The display name is the file's own `name` attribute, so a preset is
        called whatever it says it is called and the filename is free to carry
        an ordering prefix. Falls back to the filename if the attribute is
        missing, which is what a hand-written file is most likely to forget. */
    static juce::String nameOf (const juce::XmlElement* xml, const juce::String& fallback)
    {
        if (xml != nullptr)
        {
            const auto attr = xml->getStringAttribute ("name").trim();

            if (attr.isNotEmpty())
                return attr;
        }

        return fallback;
    }

    void scanBundled()
    {
        // Numbered filenames, so the order is stable — the host's program list
        // is indexed, and a preset changing index would repoint saved sessions.
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String original (BinaryData::originalFilenames[i]);

            if (! original.startsWithIgnoreCase ("preset_") || ! original.endsWithIgnoreCase (".xml"))
                continue;

            const juce::String fallback = original.upToLastOccurrenceOf (".", false, false)
                                                  .fromFirstOccurrenceOf ("_", false, false)
                                                  .fromFirstOccurrenceOf ("_", false, false)
                                                  .replaceCharacter ('_', ' ');

            int size = 0;
            std::unique_ptr<juce::XmlElement> xml;

            if (auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                xml = juce::parseXML (juce::String::createStringFromData (data, size));

            bundledPresets.add ({ nameOf (xml.get(), fallback), {}, i });
        }

        std::sort (bundledPresets.begin(), bundledPresets.end(),
                   [] (const Entry& a, const Entry& b)
                   {
                       return juce::String (BinaryData::originalFilenames[a.binaryIndex])
                                .compareIgnoreCase (BinaryData::originalFilenames[b.binaryIndex]) < 0;
                   });
    }

    void apply (const juce::XmlElement& xml)
    {
        for (auto* p : processor.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);

            if (ranged == nullptr)
                continue;

            const auto id = ranged->getParameterID();

            // Default, not the current value: a preset missing an entry must
            // still land somewhere predictable.
            float normalised = ranged->getDefaultValue();

            if (id == chimeID)
            {
                normalised = 0.0f;
            }
            else
            {
                for (auto* e : xml.getChildWithTagNameIterator ("PARAM"))
                {
                    if (e->getStringAttribute ("id") == id)
                    {
                        normalised = ranged->getNormalisableRange().convertTo0to1 (
                            (float) e->getDoubleAttribute ("value"));
                        break;
                    }
                }
            }

            // Through the parameter, not into the tree, so the host sees it.
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
            ranged->endChangeGesture();
        }
    }

    static constexpr const char* chimeID = "chime";

    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    juce::Array<Entry> bundledPresets, userPresets;
    juce::String current;
    int currentBundled = -1;
    int rev = 0;
};

inline bool operator== (const PresetManager::Entry& a, const PresetManager::Entry& b)
{
    return a.name == b.name && a.binaryIndex == b.binaryIndex && a.file == b.file;
}
