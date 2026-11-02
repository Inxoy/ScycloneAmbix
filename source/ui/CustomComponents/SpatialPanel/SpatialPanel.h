//
// Controls and live view of the latent driven ambisonic sources.
//

#ifndef SCYCLONE_SPATIALPANEL_H
#define SCYCLONE_SPATIALPANEL_H

#include "JuceHeader.h"
#include "../ParameterControl/Slider/CustomSliderComponent.h"
#include "../../LookAndFeel/CustomFontLookAndFeel.h"
#include "../../../utils/colors.h"
#include "../../../PluginParameters.h"
#include "../../../PluginProcessor.h"

// top view of the sphere: front is up, left is left, the distance from the centre shows
// the elevation (edge = horizon, centre = straight up or down)
class SpatialView : public juce::Component {
public:
    SpatialView(AudioPluginAudioProcessor& processor, juce::AudioProcessorValueTreeState& parameters);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void update();
    void setSelectedSource(int network);
    std::function<void(int network)> onSourceSelected;

private:
    juce::Point<float> toScreen(float azimuthDegrees, float elevationDegrees) const;
    juce::Rectangle<float> getSphereBounds() const;
    void setBasePositionFromMouse(juce::Point<float> position);

    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;

    static constexpr int trailLength = 48;
    struct Trail {
        std::array<juce::Point<float>, trailLength> positions;  // azimuth, elevation
        int writeIndex = 0;
        int size = 0;
    };
    std::array<Trail, 2> trails;
    int selectedSource = 1;
    int draggedSource = 0;
};

class SpatialPanel : public juce::Component, private juce::Timer {
public:
    SpatialPanel(AudioPluginAudioProcessor& processor, juce::AudioProcessorValueTreeState& parameters);
    ~SpatialPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void selectSource(int network);

    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;

    SpatialView spatialView;

    juce::TextButton sourceButton1 {"Source 1"};
    juce::TextButton sourceButton2 {"Source 2"};
    int selectedSource = 1;

    juce::Label latentLabels[3];
    juce::ComboBox latentBoxes[3];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> latentAttachments[3];

    CustomSliderComponent azimuthSlider {"Azimuth"};
    CustomSliderComponent elevationSlider {"Elevation"};
    CustomSliderComponent spreadSlider {"Spread"};
    CustomSliderComponent azimuthDepthSlider {"Az Mod"};
    CustomSliderComponent elevationDepthSlider {"El Mod"};
    CustomSliderComponent spreadDepthSlider {"Spr Mod"};
    CustomSliderComponent smoothingSlider {"Smooth"};
    CustomSliderComponent dryWidthSlider {"Dry Width"};

    CustomSliderComponent* sliders[8] = {&azimuthSlider, &elevationSlider, &spreadSlider,
                                         &azimuthDepthSlider, &elevationDepthSlider, &spreadDepthSlider,
                                         &smoothingSlider, &dryWidthSlider};

    // Geometry, matches the advanced parameter view
    const int sliderWidth = 73;
    const int sliderHeight = 230;
    const int sliderDistance = 2;
    const float lineHeight = 3.f;
    const float textHeight = 20.f;

    juce::Rectangle<float> sourceText, sourceLine, globalText, globalLine, latentText, latentLine;
    juce::Rectangle<float> latentBarsArea;
    juce::Rectangle<float> statusArea;
};

#endif //SCYCLONE_SPATIALPANEL_H
