// Renders the editor's pages to PNG files, to look at the interface without a host:
//   ScycloneUiSnapshot <output folder>
// writes networks.png and space.png.

#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                    : juce::File::getCurrentWorkingDirectory();
    out.createDirectory();

    AudioPluginAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // models found in the Models folders; the first one goes into network II
    const auto names = ModelLibrary::getNames();
    std::cout << "models: " << names.joinIntoString (", ") << std::endl;
    if (names.size() > 2)
    {
        processor.selectModel (2, 2);
        std::cout << "network II now runs: " << processor.getModelName (2) << std::endl;
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    auto save = [&] (const juce::String& name)
    {
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream stream (out.getChildFile (name));
        stream.setPosition (0);
        stream.truncate();
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::cout << "wrote " << out.getChildFile (name).getFullPathName() << std::endl;
    };

    auto clickTab = [&] (const juce::String& text)
    {
        for (auto* c : editor->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getButtonText() == text && b->onClick) b->onClick();
    };

    clickTab ("NETWORKS");
    save ("networks.png");
    clickTab ("SPACE");
    save ("space.png");
    clickTab ("NETWORKS");

    editor.reset();
    processor.releaseResources();
    return 0;
}
