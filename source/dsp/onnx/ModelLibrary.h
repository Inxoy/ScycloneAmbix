#pragma once

#include "JuceHeader.h"
#include "InferenceThread.h"

/** The networks a slot can load: the two models compiled into the plugin, then every .ort
    file in a models folder (the RAVE models converted with tools/convert_rave_ts.py).

    Models folders, searched in this order (a name found twice is listed once):
      - <VST3 bundle>/Contents/Resources/Models
      - <folder of the plugin or standalone binary>/Models
      - <user application data>/ScycloneAmbix/Models  (%APPDATA% on Windows,
        ~/Library on macOS) */
class ModelLibrary {
public:
    struct Entry {
        juce::String name;      // shown in the UI and stored in the session
        bool embedded = false;
        RaveModel embeddedModel = FunkDrum;
        juce::File file;
    };

    static const juce::Array<Entry>& getEntries() {
        static const juce::Array<Entry> entries = scan();
        return entries;
    }

    static int indexOf(const juce::String& name) {
        const auto& e = getEntries();
        for (int i = 0; i < e.size(); ++i)
            if (e.getReference(i).name == name) return i;
        return -1;
    }

    static juce::StringArray getNames() {
        juce::StringArray names;
        for (const auto& e : getEntries()) names.add(e.name);
        return names;
    }

    static juce::Array<juce::File> getModelFolders() {
        juce::Array<juce::File> folders;
        const auto binary = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        // VST3: <name>.vst3/Contents/<arch>/<name>.vst3, AU/VST3 on macOS: Contents/MacOS/<name>
        folders.add(binary.getParentDirectory().getSiblingFile("Resources").getChildFile("Models"));
        folders.add(binary.getParentDirectory().getChildFile("Models"));
        folders.add(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("ScycloneAmbix").getChildFile("Models"));
        return folders;
    }

    /** "sol_ordinario" -> "Sol Ordinario", "VCTK" stays */
    static juce::String displayName(const juce::String& fileStem) {
        if (fileStem == fileStem.toUpperCase()) return fileStem;
        juce::StringArray words;
        words.addTokens(fileStem.replaceCharacters("_-", "  "), " ", "");
        words.removeEmptyStrings();
        for (auto& w : words) w = w.substring(0, 1).toUpperCase() + w.substring(1);
        return words.joinIntoString(" ");
    }

private:
    static juce::Array<Entry> scan() {
        juce::Array<Entry> entries;
        entries.add({"Funk", true, FunkDrum, {}});
        entries.add({"Djembe", true, Djembe, {}});

        juce::StringArray seen {"Funk", "Djembe"};
        for (const auto& folder : getModelFolders()) {
            if (!folder.isDirectory()) continue;
            auto files = folder.findChildFiles(juce::File::findFiles, false, "*.ort");
            files.sort();
            for (const auto& f : files) {
                const auto name = displayName(f.getFileNameWithoutExtension());
                if (seen.contains(name)) continue;
                seen.add(name);
                entries.add({name, false, FunkDrum, f});
            }
        }
        return entries;
    }
};
