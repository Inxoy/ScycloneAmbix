#pragma once

// The editor's pages in the rysum look: two network columns around the field pad,
// the master section underneath, and the space page for the ambisonic sources.

#include "JuceHeader.h"
#include "../Theme/Theme.h"
#include "../../PluginParameters.h"
#include "../../PluginProcessor.h"

namespace scyclone::ui
{
/** "I" / "II" */
inline juce::String networkNumeral (int network) { return network == 1 ? "I" : "II"; }

/** the marker of a network: network I a filled lozenge, network II a hollow one */
void drawNetworkMarker (juce::Graphics& g, int network, juce::Point<float> c, float size, juce::Colour colour);

//==============================================================================
class NetworkColumn : public juce::Component
{
public:
    NetworkColumn (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& state, int network);

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** shows the model the processor has loaded */
    void refreshModel();
    std::function<void (int network)> onLoadFile;

private:
    void updateGrainEnabled();

    AudioPluginAudioProcessor& proc;
    const int network;
    juce::ComboBox modelBox;
    ParamToggle power, grain;
    ParamRow transient, filter, attack, interval, size, pitch, grainMix;
    juce::Rectangle<int> titleArea, grainTitleArea;

    static constexpr int loadFileId = 1000;
};

//==============================================================================
/** the two networks as points: x = transient shaper, y = filter; the lozenges swell with the level */
class FieldPad : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    FieldPad (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& state);
    ~FieldPad() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> field() const;
    juce::Point<float> markerPosition (int network) const;
    void setFromMouse (juce::Point<float> pos);
    juce::RangedAudioParameter* xParam (int network) const;
    juce::RangedAudioParameter* yParam (int network) const;
    bool isOn (int network) const;

    AudioPluginAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    float level[2] {};
    int dragged = 0;
};

//==============================================================================
class MasterSection : public juce::Component
{
public:
    explicit MasterSection (juce::AudioProcessorValueTreeState& state);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ParamRow input, fade, mix, output, dynamic, threshold, ratio, makeup;
};

//==============================================================================
/** top view of the sphere: front is up, the distance from the centre shows the elevation */
class SphereView : public juce::Component, public juce::SettableTooltipClient
{
public:
    SphereView (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& state);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void update();
    void setSelectedSource (int network) { selected = network; repaint(); }
    std::function<void (int network)> onSourceSelected;

private:
    juce::Rectangle<float> sphere() const;
    juce::Point<float> toScreen (float azimuthDegrees, float elevationDegrees) const;
    void setBaseFromMouse (juce::Point<float> pos);

    AudioPluginAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    static constexpr int trailLength = 48;
    struct Trail { std::array<juce::Point<float>, trailLength> positions; int writeIndex = 0, size = 0; };
    std::array<Trail, 2> trails;
    int selected = 1, dragged = 0;
};

class SpacePage : public juce::Component, private juce::Timer
{
public:
    SpacePage (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& state);
    ~SpacePage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    struct SourceControls
    {
        SourceControls (juce::AudioProcessorValueTreeState& state, int network);
        std::unique_ptr<ParamRow> azimuth, elevation, spread, azimuthDepth, elevationDepth, spreadDepth;
        juce::ComboBox latent[3];
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> latentAttachment[3];
        juce::Array<juce::Component*> all();
    };

    void timerCallback() override;
    void selectSource (int network);

    AudioPluginAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    SphereView sphereView;
    juce::TextButton sourceButton[2];
    SourceControls source1, source2;
    ParamRow smoothing, dryWidth;
    int selected = 1;
    juce::Rectangle<int> sourceTitle, latentTitle, globalTitle, latentBars, latentRows, statusArea;
};
} // namespace scyclone::ui
