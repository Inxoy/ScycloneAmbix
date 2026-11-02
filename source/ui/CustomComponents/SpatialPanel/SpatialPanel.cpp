//
// Controls and live view of the latent driven ambisonic sources.
//

#include "SpatialPanel.h"

namespace {
    const juce::Colour sourceColours[2] = {juce::Colour {0xffF02FC2}, juce::Colour {0xffFD8263}};
    const juce::Colour sourceColoursBottom[2] = {juce::Colour {0xff6094EA}, juce::Colour {0xff74276C}};
    const juce::Colour lineColour {0xff181819};
    const juce::Colour headingColour {0xff757677};
    const juce::Colour gridColour {0x32CECECE};

    struct SourceParameterIDs {
        juce::String azimuth, elevation, spread, azimuthDepth, elevationDepth, spreadDepth,
                azimuthLatent, elevationLatent, spreadLatent;
    };

    SourceParameterIDs getParameterIDs(int network) {
        if (network == 2)
            return {PluginParameters::AMBI_AZIMUTH_NETWORK2_ID.getParamID(),
                    PluginParameters::AMBI_ELEVATION_NETWORK2_ID.getParamID(),
                    PluginParameters::AMBI_SPREAD_NETWORK2_ID.getParamID(),
                    PluginParameters::AMBI_AZIMUTH_DEPTH_NETWORK2_ID.getParamID(),
                    PluginParameters::AMBI_ELEVATION_DEPTH_NETWORK2_ID.getParamID(),
                    PluginParameters::AMBI_SPREAD_DEPTH_NETWORK2_ID.getParamID(),
                    PluginParameters::AMBI_AZIMUTH_LATENT_NETWORK2_ID.getParamID(),
                    PluginParameters::AMBI_ELEVATION_LATENT_NETWORK2_ID.getParamID(),
                    PluginParameters::AMBI_SPREAD_LATENT_NETWORK2_ID.getParamID()};
        return {PluginParameters::AMBI_AZIMUTH_NETWORK1_ID.getParamID(),
                PluginParameters::AMBI_ELEVATION_NETWORK1_ID.getParamID(),
                PluginParameters::AMBI_SPREAD_NETWORK1_ID.getParamID(),
                PluginParameters::AMBI_AZIMUTH_DEPTH_NETWORK1_ID.getParamID(),
                PluginParameters::AMBI_ELEVATION_DEPTH_NETWORK1_ID.getParamID(),
                PluginParameters::AMBI_SPREAD_DEPTH_NETWORK1_ID.getParamID(),
                PluginParameters::AMBI_AZIMUTH_LATENT_NETWORK1_ID.getParamID(),
                PluginParameters::AMBI_ELEVATION_LATENT_NETWORK1_ID.getParamID(),
                PluginParameters::AMBI_SPREAD_LATENT_NETWORK1_ID.getParamID()};
    }

    bool isNetworkActive(juce::AudioProcessorValueTreeState& apvts, int network) {
        const auto& id = network == 1 ? PluginParameters::ON_OFF_NETWORK1_ID : PluginParameters::ON_OFF_NETWORK2_ID;
        return apvts.getRawParameterValue(id.getParamID())->load() > 0.5f;
    }
}

//==============================================================================
SpatialView::SpatialView(AudioPluginAudioProcessor &processor, juce::AudioProcessorValueTreeState &parameters)
        : processorRef(processor), apvts(parameters) {
}

juce::Rectangle<float> SpatialView::getSphereBounds() const {
    auto bounds = getLocalBounds().toFloat().reduced(22.f);
    const auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    return bounds.withSizeKeepingCentre(size, size);
}

juce::Point<float> SpatialView::toScreen(float azimuthDegrees, float elevationDegrees) const {
    const auto sphere = getSphereBounds();
    const auto radius = sphere.getWidth() * 0.5f;
    const auto azimuth = juce::degreesToRadians(azimuthDegrees);
    const auto horizontal = std::cos(juce::degreesToRadians(elevationDegrees));
    return {sphere.getCentreX() - radius * horizontal * std::sin(azimuth),
            sphere.getCentreY() - radius * horizontal * std::cos(azimuth)};
}

void SpatialView::update() {
    for (int network = 1; network <= 2; ++network) {
        auto& trail = trails[(size_t) network - 1];
        trail.positions[(size_t) trail.writeIndex] = {processorRef.getAmbisonicAzimuth(network),
                                                      processorRef.getAmbisonicElevation(network)};
        trail.writeIndex = (trail.writeIndex + 1) % trailLength;
        trail.size = juce::jmin(trail.size + 1, trailLength);
    }
    repaint();
}

void SpatialView::setSelectedSource(int network) {
    selectedSource = network;
    repaint();
}

void SpatialView::paint(juce::Graphics &g) {
    const auto sphere = getSphereBounds();
    const auto centre = sphere.getCentre();

    // horizon, 30 and 60 degree elevation rings, cross
    g.setColour(juce::Colour {0xff161617});
    g.fillEllipse(sphere);
    g.setColour(gridColour);
    g.drawEllipse(sphere, 1.5f);
    for (auto elevation : {30.f, 60.f}) {
        const auto r = sphere.getWidth() * 0.5f * std::cos(juce::degreesToRadians(elevation));
        g.drawEllipse(juce::Rectangle<float>(2.f * r, 2.f * r).withCentre(centre), 0.8f);
    }
    g.drawLine(sphere.getX(), centre.y, sphere.getRight(), centre.y, 0.8f);
    g.drawLine(centre.x, sphere.getY(), centre.x, sphere.getBottom(), 0.8f);

    g.setFont(CustomFontLookAndFeel::getCustomFont().withHeight(12.f));
    g.setColour(headingColour);
    g.drawText("Front", juce::Rectangle<float>(60.f, 16.f).withCentre({centre.x, sphere.getY() - 11.f}), juce::Justification::centred);
    g.drawText("Back", juce::Rectangle<float>(60.f, 16.f).withCentre({centre.x, sphere.getBottom() + 11.f}), juce::Justification::centred);
    g.drawText("L", juce::Rectangle<float>(16.f, 16.f).withCentre({sphere.getX() - 11.f, centre.y}), juce::Justification::centred);
    g.drawText("R", juce::Rectangle<float>(16.f, 16.f).withCentre({sphere.getRight() + 11.f, centre.y}), juce::Justification::centred);

    // the selected source is drawn last, on top
    const int order[2] = {selectedSource == 1 ? 2 : 1, selectedSource};
    for (int network : order) {
        const auto colour = sourceColours[network - 1];
        const bool active = isNetworkActive(apvts, network);
        const float alpha = active ? 1.f : 0.3f;
        const bool selected = network == selectedSource;

        // trail of the latent movement
        const auto& trail = trails[(size_t) network - 1];
        for (int i = 1; i < trail.size; ++i) {
            const auto from = trail.positions[(size_t) ((trail.writeIndex - i - 1 + 2 * trailLength) % trailLength)];
            const auto to = trail.positions[(size_t) ((trail.writeIndex - i + 2 * trailLength) % trailLength)];
            const auto fade = 1.f - (float) i / (float) trailLength;
            g.setColour(colour.withAlpha(0.5f * fade * alpha));
            g.drawLine(juce::Line<float>(toScreen(from.x, from.y), toScreen(to.x, to.y)), 2.f);
        }

        // base position set by the parameters
        const auto ids = getParameterIDs(network);
        const auto basePosition = toScreen(apvts.getRawParameterValue(ids.azimuth)->load(),
                                           apvts.getRawParameterValue(ids.elevation)->load());
        g.setColour(colour.withAlpha(0.8f * alpha));
        g.drawEllipse(juce::Rectangle<float>(14.f, 14.f).withCentre(basePosition), selected ? 2.f : 1.f);

        // current position, sources below the horizon are drawn hollow, spread widens the halo
        const auto elevation = processorRef.getAmbisonicElevation(network);
        const auto position = toScreen(processorRef.getAmbisonicAzimuth(network), elevation);
        const auto spread = processorRef.getAmbisonicSpread(network);
        const auto haloSize = 16.f + spread * sphere.getWidth() * 0.5f;
        g.setColour(colour.withAlpha(0.18f * alpha));
        g.fillEllipse(juce::Rectangle<float>(haloSize, haloSize).withCentre(position));

        const auto dot = juce::Rectangle<float>(12.f, 12.f).withCentre(position);
        g.setColour(colour.withAlpha(alpha));
        if (elevation >= 0.f) g.fillEllipse(dot);
        else g.drawEllipse(dot, 2.f);

        g.setColour(juce::Colour::fromString(ColorPallete::WHITE).withAlpha(0.85f * alpha));
        g.setFont(CustomFontLookAndFeel::getCustomFontBold().withHeight(11.f));
        g.drawText(juce::String(network), dot.translated(10.f, -10.f), juce::Justification::centred);
    }
}

void SpatialView::mouseDown(const juce::MouseEvent &event) {
    // grab the source whose base or current position is closest, otherwise the selected one
    const auto position = event.position;
    float closest = 24.f;
    draggedSource = 0;
    for (int network = 1; network <= 2; ++network) {
        const auto ids = getParameterIDs(network);
        const auto base = toScreen(apvts.getRawParameterValue(ids.azimuth)->load(),
                                   apvts.getRawParameterValue(ids.elevation)->load());
        const auto current = toScreen(processorRef.getAmbisonicAzimuth(network), processorRef.getAmbisonicElevation(network));
        const auto distance = juce::jmin(base.getDistanceFrom(position), current.getDistanceFrom(position));
        if (distance < closest) {
            closest = distance;
            draggedSource = network;
        }
    }
    if (draggedSource == 0) {
        if (!getSphereBounds().contains(position)) return;
        draggedSource = selectedSource;
    }

    if (draggedSource != selectedSource && onSourceSelected) onSourceSelected(draggedSource);

    const auto ids = getParameterIDs(draggedSource);
    apvts.getParameter(ids.azimuth)->beginChangeGesture();
    apvts.getParameter(ids.elevation)->beginChangeGesture();
    setBasePositionFromMouse(position);
}

void SpatialView::mouseDrag(const juce::MouseEvent &event) {
    if (draggedSource != 0) setBasePositionFromMouse(event.position);
}

void SpatialView::mouseUp(const juce::MouseEvent &) {
    if (draggedSource == 0) return;
    const auto ids = getParameterIDs(draggedSource);
    apvts.getParameter(ids.azimuth)->endChangeGesture();
    apvts.getParameter(ids.elevation)->endChangeGesture();
    draggedSource = 0;
}

void SpatialView::setBasePositionFromMouse(juce::Point<float> position) {
    const auto sphere = getSphereBounds();
    const auto offset = position - sphere.getCentre();
    const auto radius = sphere.getWidth() * 0.5f;
    const auto horizontal = juce::jlimit(0.f, 1.f, offset.getDistanceFromOrigin() / radius);

    const auto ids = getParameterIDs(draggedSource);
    auto* azimuthParameter = apvts.getParameter(ids.azimuth);
    auto* elevationParameter = apvts.getParameter(ids.elevation);

    // the view can't tell up from down, so the sign of the elevation is kept
    const auto azimuth = juce::radiansToDegrees(std::atan2(-offset.x, -offset.y));
    const auto previousElevation = apvts.getRawParameterValue(ids.elevation)->load();
    const auto elevation = std::copysign(juce::radiansToDegrees(std::acos(horizontal)), previousElevation < 0.f ? -1.f : 1.f);

    azimuthParameter->setValueNotifyingHost(azimuthParameter->convertTo0to1(azimuth));
    elevationParameter->setValueNotifyingHost(elevationParameter->convertTo0to1(elevation));
    repaint();
}

//==============================================================================
SpatialPanel::SpatialPanel(AudioPluginAudioProcessor &processor, juce::AudioProcessorValueTreeState &parameters)
        : processorRef(processor), apvts(parameters), spatialView(processor, parameters) {
    addAndMakeVisible(spatialView);
    spatialView.onSourceSelected = [this] (int network) { selectSource(network); };

    for (auto* button : {&sourceButton1, &sourceButton2}) {
        button->setClickingTogglesState(true);
        button->setRadioGroupId(1);
        button->setColour(juce::TextButton::buttonColourId, juce::Colour::fromString(ColorPallete::BUTTON_GRAY));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour::fromString(ColorPallete::TEXT2));
        button->setColour(juce::TextButton::textColourOnId, juce::Colour::fromString(ColorPallete::WHITE));
        addAndMakeVisible(button);
    }
    sourceButton1.setColour(juce::TextButton::buttonOnColourId, sourceColours[0].withAlpha(0.45f));
    sourceButton2.setColour(juce::TextButton::buttonOnColourId, sourceColours[1].withAlpha(0.45f));
    sourceButton1.onClick = [this] { if (sourceButton1.getToggleState()) selectSource(1); };
    sourceButton2.onClick = [this] { if (sourceButton2.getToggleState()) selectSource(2); };

    const juce::String latentNames[3] = {"Azimuth", "Elevation", "Spread"};
    for (int i = 0; i < 3; ++i) {
        latentLabels[i].setText(latentNames[i], juce::dontSendNotification);
        latentLabels[i].setFont(CustomFontLookAndFeel::getCustomFont().withHeight(13.5f));
        latentLabels[i].setColour(juce::Label::textColourId, juce::Colour::fromString(ColorPallete::TEXT2));
        addAndMakeVisible(latentLabels[i]);

        latentBoxes[i].addItemList(PluginParameters::getLatentDimensionChoices(), 1);
        latentBoxes[i].setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromString(ColorPallete::BUTTON_GRAY));
        latentBoxes[i].setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        latentBoxes[i].setColour(juce::ComboBox::textColourId, juce::Colour {0xffE2D1C3});
        latentBoxes[i].setColour(juce::ComboBox::arrowColourId, juce::Colour::fromString(ColorPallete::TEXT2));
        addAndMakeVisible(latentBoxes[i]);
    }

    smoothingSlider.addSliderAttachment(parameters, PluginParameters::AMBI_SMOOTHING_ID.getParamID());
    dryWidthSlider.addSliderAttachment(parameters, PluginParameters::AMBI_DRY_WIDTH_ID.getParamID());
    for (auto* slider : {&smoothingSlider, &dryWidthSlider}) {
        slider->setCustomColour(CustomSliderColourID::gradientColourTopId, juce::Colour {0xff18A5A7});
        slider->setCustomColour(CustomSliderColourID::gradientColourBottomId, juce::Colour {0xffBFFFC7});
    }

    for (auto* slider : sliders) addAndMakeVisible(slider);

    sourceButton1.setToggleState(true, juce::dontSendNotification);
    selectSource(1);
}

SpatialPanel::~SpatialPanel() {
    stopTimer();
}

void SpatialPanel::selectSource(int network) {
    selectedSource = network;
    (network == 1 ? sourceButton1 : sourceButton2).setToggleState(true, juce::dontSendNotification);
    spatialView.setSelectedSource(network);

    const auto ids = getParameterIDs(network);
    azimuthSlider.addSliderAttachment(apvts, ids.azimuth);
    elevationSlider.addSliderAttachment(apvts, ids.elevation);
    spreadSlider.addSliderAttachment(apvts, ids.spread);
    azimuthDepthSlider.addSliderAttachment(apvts, ids.azimuthDepth);
    elevationDepthSlider.addSliderAttachment(apvts, ids.elevationDepth);
    spreadDepthSlider.addSliderAttachment(apvts, ids.spreadDepth);

    for (int i = 0; i < 6; ++i) {
        sliders[i]->setCustomColour(CustomSliderColourID::gradientColourTopId, sourceColours[network - 1]);
        sliders[i]->setCustomColour(CustomSliderColourID::gradientColourBottomId, sourceColoursBottom[network - 1]);
    }

    const juce::String latentIDs[3] = {ids.azimuthLatent, ids.elevationLatent, ids.spreadLatent};
    for (int i = 0; i < 3; ++i) {
        latentAttachments[i].reset();
        latentAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, latentIDs[i], latentBoxes[i]);
    }
    repaint();
}

void SpatialPanel::visibilityChanged() {
    if (isVisible()) startTimerHz(30);
    else stopTimer();
}

void SpatialPanel::timerCallback() {
    spatialView.update();
    repaint(latentBarsArea.getSmallestIntegerContainer().expanded(2));
    repaint(statusArea.getSmallestIntegerContainer());
}

void SpatialPanel::resized() {
    auto r = getLocalBounds();

    // upper section: sphere view and latent mapping of the selected source
    auto upper = r.removeFromTop(330);
    spatialView.setBounds(upper.removeFromLeft(300));
    upper.removeFromLeft(15);

    auto column = upper.reduced(0, 10);
    latentText = column.removeFromTop((int) textHeight).toFloat();
    latentLine = column.removeFromTop((int) lineHeight).toFloat();
    column.removeFromTop(12);

    auto buttons = column.removeFromTop(26);
    sourceButton1.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2 - 3));
    buttons.removeFromLeft(6);
    sourceButton2.setBounds(buttons);
    column.removeFromTop(12);

    for (int i = 0; i < 3; ++i) {
        auto row = column.removeFromTop(26);
        latentLabels[i].setBounds(row.removeFromLeft(90));
        latentBoxes[i].setBounds(row);
        column.removeFromTop(6);
    }
    column.removeFromTop(8);
    latentBarsArea = column.removeFromTop(70).toFloat();
    statusArea = column.removeFromTop(36).toFloat();

    // lower section: sliders of the selected source and global settings
    r.removeFromTop(10);
    auto headings = r.removeFromTop(30);
    const auto sourceWidth = 6 * sliderWidth + 5 * sliderDistance;
    sourceText = headings.withWidth(sourceWidth).withHeight((int) textHeight).toFloat();
    sourceLine = sourceText.translated(0.f, textHeight + 2.f).withHeight(lineHeight);
    globalText = sourceText.withX((float) (sourceWidth + 22)).withWidth((float) (2 * sliderWidth + sliderDistance));
    globalLine = globalText.translated(0.f, textHeight + 2.f).withHeight(lineHeight);

    auto lower = r.removeFromTop(sliderHeight);
    for (int i = 0; i < 8; ++i) {
        if (i == 6) lower.removeFromLeft(22 - sliderDistance);
        sliders[i]->setBounds(lower.removeFromLeft(sliderWidth));
        lower.removeFromLeft(sliderDistance);
    }
}

void SpatialPanel::paint(juce::Graphics &g) {
    g.setColour(lineColour);
    g.fillRect(latentLine);
    g.fillRect(sourceLine);
    g.fillRect(globalLine);

    g.setFont(CustomFontLookAndFeel::getCustomFontBold().withHeight(12.f));
    g.setColour(headingColour);
    g.drawText("Latent Mapping", latentText, juce::Justification::centred);
    g.drawText("Source " + juce::String(selectedSource), sourceText, juce::Justification::centred);
    g.drawText("Spatial", globalText, juce::Justification::centred);

    // live normalised latent of the selected source, mapped dimensions are highlighted
    const int numDimensions = juce::jmin(processorRef.getNumLatentDimensions(selectedSource),
                                         ProcessorAmbisonics::numDisplayedLatentDimensions);
    const auto ids = getParameterIDs(selectedSource);
    const int mapped[3] = {(int) apvts.getRawParameterValue(ids.azimuthLatent)->load(),
                           (int) apvts.getRawParameterValue(ids.elevationLatent)->load(),
                           (int) apvts.getRawParameterValue(ids.spreadLatent)->load()};
    const juce::String mappedLetters[3] = {"A", "E", "S"};

    const auto bars = latentBarsArea.withTrimmedBottom(14.f);
    const auto barWidth = bars.getWidth() / (float) ProcessorAmbisonics::numDisplayedLatentDimensions;
    const auto centreY = bars.getCentreY();
    g.setColour(gridColour);
    g.drawHorizontalLine((int) centreY, bars.getX(), bars.getRight());

    for (int dim = 0; dim < ProcessorAmbisonics::numDisplayedLatentDimensions; ++dim) {
        const auto x = bars.getX() + (float) dim * barWidth;
        int mappedIndex = -1;
        for (int i = 0; i < 3; ++i) if (mapped[i] == dim) mappedIndex = i;

        const auto colour = mappedIndex >= 0 ? sourceColours[selectedSource - 1] : juce::Colour::fromString(ColorPallete::TEXT2);
        if (dim < numDimensions) {
            const auto value = processorRef.getNormalisedLatent(selectedSource, dim);
            const auto height = std::abs(value) * bars.getHeight() * 0.5f;
            g.setColour(colour.withAlpha(mappedIndex >= 0 ? 1.f : 0.45f));
            g.fillRect(juce::Rectangle<float>(x + 2.f, value >= 0.f ? centreY - height : centreY, barWidth - 4.f, juce::jmax(1.f, height)));
        }
        if (mappedIndex >= 0) {
            g.setColour(colour);
            g.setFont(CustomFontLookAndFeel::getCustomFontBold().withHeight(10.f));
            g.drawText(mappedLetters[mappedIndex], juce::Rectangle<float>(x, bars.getBottom() + 1.f, barWidth, 12.f), juce::Justification::centred);
        }
    }

    // output format and latent availability
    juce::String status;
    const int order = processorRef.getAmbisonicOrder();
    if (order > 0) {
        const juce::String orderNames[4] = {"", "1st", "2nd", "3rd"};
        status = orderNames[juce::jlimit(1, 3, order)] + " order AmbiX, " + juce::String((order + 1) * (order + 1)) + " channels";
    } else {
        status = "Stereo output, spatial encoding inactive";
    }
    if (!isNetworkActive(apvts, selectedSource))
        status += "\nSource " + juce::String(selectedSource) + " is switched off";
    else if (processorRef.getNumLatentDimensions(selectedSource) == 0)
        status += "\nNo latent from this model yet";
    else
        status += "\n" + juce::String(processorRef.getNumLatentDimensions(selectedSource)) + " latent dimensions";

    g.setFont(CustomFontLookAndFeel::getCustomFont().withHeight(12.f));
    g.setColour(headingColour);
    g.drawFittedText(status, statusArea.getSmallestIntegerContainer(), juce::Justification::centredLeft, 2);
}
