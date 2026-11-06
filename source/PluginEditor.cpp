#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginParameters.h"

using namespace scyclone::ui;

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& parameters)
    : AudioProcessorEditor (&p), processorRef (p), apvts (parameters),
      network1 (p, parameters, 1), network2 (p, parameters, 2),
      fieldPad (p, parameters), master (parameters), space (p, parameters)
{
    setLookAndFeel (&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);     // popup menus and alert windows

    apvts.addParameterListener (PluginParameters::SELECT_NETWORK1_ID.getParamID(), this);
    apvts.addParameterListener (PluginParameters::SELECT_NETWORK2_ID.getParamID(), this);

    for (auto* c : { (juce::Component*) &network1, (juce::Component*) &network2, (juce::Component*) &fieldPad, (juce::Component*) &master })
        addAndMakeVisible (c);
    addChildComponent (space);

    network1.onLoadFile = network2.onLoadFile = [this] (int n) { openFileChooser (n); };
    processorRef.onModelChanged = [safe = juce::Component::SafePointer<AudioPluginAudioProcessorEditor> (this)]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (safe == nullptr) return;
            safe->network1.refreshModel();
            safe->network2.refreshModel();
        });
    };

    const char* names[] { "Networks", "Space" };
    for (int i = 0; i < 2; ++i)
    {
        tabs[i].setButtonText (juce::String (names[i]).toUpperCase());
        tabs[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[i]);
    }
    tabs[0].setTooltip ("The two networks, the field and the master section");
    tabs[1].setTooltip ("Where the networks sound in the ambisonic field and how the latent moves them");
    showPage (processorRef.advancedParameterControlVisible.getValue() ? 1 : 0);

    // grain over the whole interface and darkened edges, like an old print; rendered once
    grainLayer = juce::Image (juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g (grainLayer);
        drawGrain (g, { 0, 0, width, height }, 1.0f);
    }
    vignette = juce::Image (juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g (vignette);
        juce::ColourGradient grad (juce::Colours::transparentBlack, width * 0.5f, height * 0.48f,
                                   juce::Colours::black.withAlpha (0.22f), 0.0f, 0.0f, true);
        grad.addColour (0.7, juce::Colours::transparentBlack);
        g.setGradientFill (grad);
        g.fillRect (0, 0, width, height);
    }

    setSize (width, height);
    startTimerHz (4);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    stopTimer();
    processorRef.onModelChanged = nullptr;
    apvts.removeParameterListener (PluginParameters::SELECT_NETWORK1_ID.getParamID(), this);
    apvts.removeParameterListener (PluginParameters::SELECT_NETWORK2_ID.getParamID(), this);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AudioPluginAudioProcessorEditor::showPage (int newPage)
{
    page = newPage;
    // the old "detail view" flag now remembers the page
    processorRef.advancedParameterControlVisible = page == 1;
    for (int i = 0; i < 2; ++i) tabs[i].setToggleState (i == page, juce::dontSendNotification);
    for (auto* c : { (juce::Component*) &network1, (juce::Component*) &network2, (juce::Component*) &fieldPad, (juce::Component*) &master })
        c->setVisible (page == 0);
    space.setVisible (page == 1);
}

juce::String AudioPluginAudioProcessorEditor::outputDescription() const
{
    const int order = processorRef.getAmbisonicOrder();
    if (order > 0) return "AmbiX order " + juce::String (order) + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String ((order + 1) * (order + 1)) + " ch";
    return "stereo";
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    const auto o = outputDescription();
    if (o != shownOutput) { shownOutput = o; repaint (0, 0, width, headerHeight); }
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto& A = Assets::get();
    g.fillAll (col::bg);
    g.setColour (col::border);
    g.fillRect (0, headerHeight - 4, width, 1);
    g.setColour (col::line);
    g.fillRect (0, headerHeight - 1, width, 1);

    // "Scyclone" with a rubricated S, "ambix" in italic after it
    auto head = juce::Rectangle<int> (0, 0, width, headerHeight).reduced (28, 0);
    const auto big = A.title (34.0f);
    const int sw = juce::roundToInt (big.getStringWidthFloat ("S"));
    const int rest = juce::roundToInt (big.getStringWidthFloat ("cyclone"));
    g.setFont (big);
    g.setColour (col::rubric.brighter (0.2f));
    g.drawText ("S", head.withWidth (sw + 4).translated (0, 2), juce::Justification::centredLeft);
    g.setColour (col::ink);
    g.drawText ("cyclone", head.withTrimmedLeft (sw).withWidth (rest + 4).translated (0, 2), juce::Justification::centredLeft);
    g.setFont (A.italic (17.0f));
    g.setColour (col::mute);
    g.drawText ("ambix", head.withTrimmedLeft (sw + rest + 8).withWidth (80).translated (0, 6), juce::Justification::centredLeft);

    g.setFont (A.italic (12.0f));
    g.setColour (col::faint);
    g.drawText (shownOutput.isEmpty() ? outputDescription() : shownOutput, head.withTrimmedLeft (head.getWidth() - 220),
                juce::Justification::centredRight);
}

void AudioPluginAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    g.drawImageAt (vignette, 0, 0);
    g.setOpacity (0.22f);
    g.drawImageAt (grainLayer, 0, 0);
}

void AudioPluginAudioProcessorEditor::resized()
{
    int x = 236;
    const int tw[] { 104, 80 };
    for (int i = 0; i < 2; ++i) { tabs[i].setBounds (x, 13, tw[i], 30); x += tw[i] + 3; }

    auto body = getLocalBounds().withTrimmedTop (headerHeight).reduced (28, 18);
    space.setBounds (getLocalBounds().withTrimmedTop (headerHeight));

    network1.setBounds (body.removeFromLeft (300));
    network2.setBounds (body.removeFromRight (300));
    body.reduce (28, 0);
    master.setBounds (body.removeFromBottom (34 + 4 * (ParamRow::preferredHeight + 2)));
    body.removeFromBottom (14);
    fieldPad.setBounds (body);
}

void AudioPluginAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // a host (or an old session) can still ask for the file chooser through these
    if (newValue < 0.5f) return;
    const int n = parameterID == PluginParameters::SELECT_NETWORK1_ID.getParamID() ? 1
                : parameterID == PluginParameters::SELECT_NETWORK2_ID.getParamID() ? 2 : 0;
    if (n == 0) return;
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<AudioPluginAudioProcessorEditor> (this), n]
    {
        if (safe != nullptr) safe->openFileChooser (n);
    });
}

void AudioPluginAudioProcessorEditor::openFileChooser (int networkID)
{
    fc = std::make_unique<juce::FileChooser> ("Load a RAVE model (.ort) into network " + networkNumeral (networkID),
                                              juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.ort", true);
    fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                     [this, networkID] (const juce::FileChooser& chooser)
                     {
                         const auto chosen = chooser.getResult();
                         if (chosen.existsAsFile() && chosen.getSize() > 0)
                             processorRef.loadExternalModel (chosen, networkID);
                     });
}
