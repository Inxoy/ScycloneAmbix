#pragma once

#include "PluginProcessor.h"
#include "PluginParameters.h"
#include "ui/Theme/Theme.h"
#include "ui/Views/ScycloneViews.h"

//==============================================================================
class AudioPluginAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                         private juce::AudioProcessorValueTreeState::Listener,
                                         private juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&, juce::AudioProcessorValueTreeState& parameters);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    static constexpr int width = 1280, height = 720, headerHeight = 56;

private:
    void openFileChooser (int networkID);
    void showPage (int page);
    void timerCallback() override;
    juce::String outputDescription() const;

    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;

    scyclone::ui::LookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    scyclone::ui::NetworkColumn network1, network2;
    scyclone::ui::FieldPad fieldPad;
    scyclone::ui::MasterSection master;
    scyclone::ui::SpacePage space;
    juce::TextButton tabs[2];
    int page = 0;
    juce::String shownOutput;

    juce::Image grainLayer, vignette;
    std::unique_ptr<juce::FileChooser> fc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
