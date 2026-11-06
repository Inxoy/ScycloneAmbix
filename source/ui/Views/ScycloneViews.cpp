#include "ScycloneViews.h"
#include "../../dsp/onnx/ModelLibrary.h"

namespace scyclone::ui
{
namespace
{
    using P = PluginParameters;

    struct SourceIDs
    {
        juce::ParameterID azimuth, elevation, spread, azimuthDepth, elevationDepth, spreadDepth,
                          azimuthLatent, elevationLatent, spreadLatent;
    };

    SourceIDs sourceIDs (int network)
    {
        if (network == 2)
            return { P::AMBI_AZIMUTH_NETWORK2_ID, P::AMBI_ELEVATION_NETWORK2_ID, P::AMBI_SPREAD_NETWORK2_ID,
                     P::AMBI_AZIMUTH_DEPTH_NETWORK2_ID, P::AMBI_ELEVATION_DEPTH_NETWORK2_ID, P::AMBI_SPREAD_DEPTH_NETWORK2_ID,
                     P::AMBI_AZIMUTH_LATENT_NETWORK2_ID, P::AMBI_ELEVATION_LATENT_NETWORK2_ID, P::AMBI_SPREAD_LATENT_NETWORK2_ID };
        return { P::AMBI_AZIMUTH_NETWORK1_ID, P::AMBI_ELEVATION_NETWORK1_ID, P::AMBI_SPREAD_NETWORK1_ID,
                 P::AMBI_AZIMUTH_DEPTH_NETWORK1_ID, P::AMBI_ELEVATION_DEPTH_NETWORK1_ID, P::AMBI_SPREAD_DEPTH_NETWORK1_ID,
                 P::AMBI_AZIMUTH_LATENT_NETWORK1_ID, P::AMBI_ELEVATION_LATENT_NETWORK1_ID, P::AMBI_SPREAD_LATENT_NETWORK1_ID };
    }

    const juce::ParameterID& pick (int network, const juce::ParameterID& one, const juce::ParameterID& two)
    {
        return network == 1 ? one : two;
    }

    bool networkOn (juce::AudioProcessorValueTreeState& s, int network)
    {
        return s.getRawParameterValue (pick (network, P::ON_OFF_NETWORK1_ID, P::ON_OFF_NETWORK2_ID).getParamID())->load() > 0.5f;
    }

    /** thin double frame like a ruled plate */
    void drawPlate (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (col::border);
        g.drawRect (r, 1.0f);
        g.setColour (col::border.withAlpha (0.45f));
        g.drawRect (r.reduced (3.0f), 0.6f);
    }
}

void drawNetworkMarker (juce::Graphics& g, int network, juce::Point<float> c, float size, juce::Colour colour)
{
    juce::Path p;
    const float rw = size * 0.62f, rh = size;
    p.startNewSubPath (c.x, c.y - rh); p.lineTo (c.x + rw, c.y); p.lineTo (c.x, c.y + rh); p.lineTo (c.x - rw, c.y); p.closeSubPath();
    g.setColour (colour);
    if (network == 1) g.fillPath (p);
    else g.strokePath (p, juce::PathStrokeType (1.6f));
}

//==============================================================================
NetworkColumn::NetworkColumn (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& s, int n)
    : proc (p), network (n),
      power (s, pick (n, P::ON_OFF_NETWORK1_ID, P::ON_OFF_NETWORK2_ID), "On"),
      grain (s, pick (n, P::GRAIN_ON_OFF_NETWORK1_ID, P::GRAIN_ON_OFF_NETWORK2_ID), "Grain"),
      transient (s, pick (n, P::TRAN_SHAPER_NETWORK1_ID, P::TRAN_SHAPER_NETWORK2_ID), "Transient", true),
      filter (s, pick (n, P::FILTER_NETWORK1_ID, P::FILTER_NETWORK2_ID), "Filter", true),
      attack (s, pick (n, P::TRAN_ATTACK_TIME_NETWORK1_ID, P::TRAN_ATTACK_TIME_NETWORK2_ID), "Attack time"),
      interval (s, pick (n, P::GRAIN_NETWORK1_INTERVAL_ID, P::GRAIN_NETWORK2_INTERVAL_ID), "Interval"),
      size (s, pick (n, P::GRAIN_NETWORK1_SIZE_ID, P::GRAIN_NETWORK2_SIZE_ID), "Size"),
      pitch (s, pick (n, P::GRAIN_NETWORK1_PITCH_ID, P::GRAIN_NETWORK2_PITCH_ID), "Pitch", true),
      grainMix (s, pick (n, P::GRAIN_NETWORK1_MIX_ID, P::GRAIN_NETWORK2_MIX_ID), "Grain mix")
{
    modelBox.setTooltip ("The RAVE network of this side. Models in the Models folder are listed here; "
                         "\"Load file\" opens any .ort export.");
    const auto names = ModelLibrary::getNames();
    for (int i = 0; i < names.size(); ++i)
    {
        if (i == 2) modelBox.addSectionHeading ("RAVE models");
        modelBox.addItem (names[i], i + 1);
    }
    modelBox.addSeparator();
    modelBox.addItem (juce::String::fromUTF8 ("Load file\xe2\x80\xa6"), loadFileId);
    modelBox.onChange = [this]
    {
        const int id = modelBox.getSelectedId();
        if (id == loadFileId)
        {
            refreshModel();
            if (onLoadFile) onLoadFile (network);
        }
        else if (id > 0 && ModelLibrary::getNames()[id - 1] != proc.getModelName (network))
            proc.selectModel (network, id - 1);
    };
    addAndMakeVisible (modelBox);

    power.setTooltip ("Switches network " + networkNumeral (n) + " on or off");
    grain.setTooltip ("Grain delay after the network");
    grain.onStateChange = [this] { updateGrainEnabled(); };
    for (juce::Component* c : { (juce::Component*) &power, (juce::Component*) &grain, (juce::Component*) &transient,
                                (juce::Component*) &filter, (juce::Component*) &attack, (juce::Component*) &interval,
                                (juce::Component*) &size, (juce::Component*) &pitch, (juce::Component*) &grainMix })
        addAndMakeVisible (c);

    refreshModel();
    updateGrainEnabled();
}

void NetworkColumn::refreshModel()
{
    const auto name = proc.getModelName (network);
    const int index = ModelLibrary::indexOf (name);
    if (index >= 0) modelBox.setSelectedId (index + 1, juce::dontSendNotification);
    else modelBox.setText (name, juce::dontSendNotification);    // a file picked by hand
    repaint();
}

void NetworkColumn::updateGrainEnabled()
{
    const bool on = grain.getToggleState();
    for (auto* r : { &interval, &size, &pitch, &grainMix }) r->setEnabled (on);
}

void NetworkColumn::paint (juce::Graphics& g)
{
    drawSectionTitle (g, titleArea, "Network " + networkNumeral (network), "RAVE");
    drawNetworkMarker (g, network, { (float) titleArea.getRight() - 46.0f, (float) titleArea.getY() + 19.0f }, 6.0f, col::ink);
    drawSectionTitle (g, grainTitleArea, "Grain", "delay");
}

void NetworkColumn::resized()
{
    auto r = getLocalBounds();
    titleArea = r.removeFromTop (34);
    modelBox.setBounds (r.removeFromTop (28));
    r.removeFromTop (10);
    auto buttons = r.removeFromTop (26);
    power.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    grain.setBounds (buttons);
    r.removeFromTop (14);
    for (auto* row : { &transient, &filter, &attack })
    {
        row->setBounds (r.removeFromTop (ParamRow::preferredHeight));
        r.removeFromTop (4);
    }
    r.removeFromTop (12);
    grainTitleArea = r.removeFromTop (34);
    for (auto* row : { &interval, &size, &pitch, &grainMix })
    {
        row->setBounds (r.removeFromTop (ParamRow::preferredHeight));
        r.removeFromTop (4);
    }
}

//==============================================================================
FieldPad::FieldPad (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& s) : proc (p), apvts (s)
{
    setTooltip ("Drag a lozenge: left/right = transient shaper, up/down = filter. Double-click resets it.");
    startTimerHz (30);
}

FieldPad::~FieldPad() { stopTimer(); }

juce::RangedAudioParameter* FieldPad::xParam (int n) const
{
    return apvts.getParameter (pick (n, P::TRAN_SHAPER_NETWORK1_ID, P::TRAN_SHAPER_NETWORK2_ID).getParamID());
}
juce::RangedAudioParameter* FieldPad::yParam (int n) const
{
    return apvts.getParameter (pick (n, P::FILTER_NETWORK1_ID, P::FILTER_NETWORK2_ID).getParamID());
}
bool FieldPad::isOn (int n) const { return networkOn (apvts, n); }

juce::Rectangle<float> FieldPad::field() const
{
    return getLocalBounds().toFloat().reduced (30.0f, 22.0f).withTrimmedBottom (6.0f);
}

juce::Point<float> FieldPad::markerPosition (int n) const
{
    const auto f = field();
    return { f.getX() + xParam (n)->getValue() * f.getWidth(), f.getBottom() - yParam (n)->getValue() * f.getHeight() };
}

void FieldPad::timerCallback()
{
    bool changed = false;
    for (int n = 0; n < 2; ++n)
    {
        const float l = juce::jlimit (0.0f, 1.0f, proc.getCurrentLevel (n + 1));
        const float smoothed = level[n] + 0.35f * (l - level[n]);
        changed = changed || std::abs (smoothed - level[n]) > 0.002f;
        level[n] = smoothed;
    }
    if (changed) repaint();
}

void FieldPad::paint (juce::Graphics& g)
{
    auto& A = Assets::get();
    const auto f = field();
    drawPlate (g, f.expanded (10.0f));

    // a scale like on an old instrument: fine grid, the centre lines a little stronger
    for (int i = 1; i < 8; ++i)
    {
        const float t = (float) i / 8.0f;
        g.setColour (i == 4 ? col::border : col::line);
        g.fillRect (f.getX() + t * f.getWidth() - 0.5f, f.getY(), 1.0f, f.getHeight());
        g.fillRect (f.getX(), f.getY() + t * f.getHeight() - 0.5f, f.getWidth(), 1.0f);
    }
    g.setColour (col::faint);
    for (int i = 0; i <= 16; ++i)
    {
        const float x = f.getX() + (float) i / 16.0f * f.getWidth();
        g.fillRect (x - 0.5f, f.getBottom() + 4.0f, 1.0f, i % 4 == 0 ? 6.0f : 3.0f);
        const float y = f.getY() + (float) i / 16.0f * f.getHeight();
        g.fillRect (f.getX() - 4.0f - (i % 4 == 0 ? 6.0f : 3.0f), y - 0.5f, i % 4 == 0 ? 6.0f : 3.0f, 1.0f);
    }

    // axis notes in italic
    g.setFont (A.italic (11.5f));
    g.setColour (col::mute);
    const auto below = juce::Rectangle<float> (f.getX(), f.getBottom() + 10.0f, f.getWidth(), 16.0f);
    g.drawText ("sustain", below, juce::Justification::centredLeft);
    g.drawText ("transient", below, juce::Justification::centred);
    g.drawText ("attack", below, juce::Justification::centredRight);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, f.getX() - 20.0f, f.getCentreY()));
        const auto side = juce::Rectangle<float> (f.getX() - 20.0f - f.getHeight() * 0.5f, f.getCentreY() - 8.0f, f.getHeight(), 16.0f);
        g.drawText ("low", side, juce::Justification::centredLeft);
        g.drawText ("filter", side, juce::Justification::centred);
        g.drawText ("high", side, juce::Justification::centredRight);
    }

    // the fade between the networks as a ruled bar along the top
    // fade 1 = only network I (left), 0 = only network II (right)
    const float towardsII = 1.0f - apvts.getRawParameterValue (P::FADE_ID.getParamID())->load();
    g.setColour (col::ink.withAlpha (0.8f));
    g.fillRect (f.getX(), f.getY() - 14.0f, f.getWidth() * towardsII, 1.5f);
    g.setColour (col::ink.withAlpha (0.35f));
    g.fillRect (f.getX() + f.getWidth() * towardsII, f.getY() - 14.0f, f.getWidth() * (1.0f - towardsII), 1.5f);
    g.setColour (col::rubric);
    fillLozenge (g, { f.getX() + f.getWidth() * towardsII, f.getY() - 13.5f }, 3.0f, 4.5f);
    g.setFont (A.roman (12.0f));
    g.setColour (col::dim);
    g.drawText ("I", juce::Rectangle<float> (f.getX() - 22.0f, f.getY() - 22.0f, 16.0f, 16.0f), juce::Justification::centred);
    g.drawText ("II", juce::Rectangle<float> (f.getRight() + 4.0f, f.getY() - 22.0f, 18.0f, 16.0f), juce::Justification::centred);

    // the networks; the one being dragged on top
    const int order[2] { dragged == 1 ? 2 : 1, dragged == 1 ? 1 : 2 };
    for (int n : order)
    {
        const auto c = markerPosition (n);
        const bool on = isOn (n);
        const float swell = 8.0f + 18.0f * std::sqrt (level[n - 1]);
        // the level as a faint halo
        g.setColour (col::ink.withAlpha (on ? 0.07f + 0.2f * level[n - 1] : 0.03f));
        g.fillEllipse (juce::Rectangle<float> (swell * 3.2f, swell * 3.2f).withCentre (c));
        // crosshair to the scales
        g.setColour ((on ? col::dim : col::faint).withAlpha (0.35f));
        g.fillRect (c.x - 0.5f, f.getBottom() - 2.0f, 1.0f, 8.0f);
        g.fillRect (f.getX() - 6.0f, c.y - 0.5f, 8.0f, 1.0f);

        drawNetworkMarker (g, n, c, swell, on ? (dragged == n ? col::white : col::ink) : col::faint);
        g.setFont (A.roman (15.0f));
        g.setColour (on ? col::ink : col::faint);
        g.drawText (networkNumeral (n), juce::Rectangle<float> (40.0f, 18.0f).withCentre (c.translated (0.0f, -swell - 13.0f)),
                    juce::Justification::centred);
        if (dragged == n)
        {
            g.setFont (A.italic (11.5f));
            g.setColour (col::dim);
            g.drawText (xParam (n)->getCurrentValueAsText() + "  /  " + yParam (n)->getCurrentValueAsText(),
                        juce::Rectangle<float> (140.0f, 16.0f).withCentre (c.translated (0.0f, swell + 14.0f)), juce::Justification::centred);
        }
    }
}

void FieldPad::mouseDown (const juce::MouseEvent& e)
{
    // the closer lozenge, network I when they overlap
    const float d1 = markerPosition (1).getDistanceFrom (e.position), d2 = markerPosition (2).getDistanceFrom (e.position);
    dragged = d2 + 0.5f < d1 ? 2 : 1;
    xParam (dragged)->beginChangeGesture();
    yParam (dragged)->beginChangeGesture();
    if (juce::jmin (d1, d2) > 18.0f) setFromMouse (e.position);     // click in the field: jump there
    repaint();
}

void FieldPad::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged != 0) setFromMouse (e.position);
}

void FieldPad::mouseUp (const juce::MouseEvent&)
{
    if (dragged == 0) return;
    xParam (dragged)->endChangeGesture();
    yParam (dragged)->endChangeGesture();
    dragged = 0;
    repaint();
}

void FieldPad::mouseDoubleClick (const juce::MouseEvent&)
{
    const float d1 = markerPosition (1).getDistanceFrom (getMouseXYRelative().toFloat());
    const float d2 = markerPosition (2).getDistanceFrom (getMouseXYRelative().toFloat());
    const int n = d2 + 0.5f < d1 ? 2 : 1;
    for (auto* prm : { xParam (n), yParam (n) })
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getDefaultValue());
        prm->endChangeGesture();
    }
    repaint();
}

void FieldPad::setFromMouse (juce::Point<float> pos)
{
    const auto f = field();
    xParam (dragged)->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (pos.x - f.getX()) / f.getWidth()));
    yParam (dragged)->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (f.getBottom() - pos.y) / f.getHeight()));
    repaint();
}

//==============================================================================
MasterSection::MasterSection (juce::AudioProcessorValueTreeState& s)
    : input (s, P::INPUT_GAIN_ID, "Input", true),
      fade (s, P::FADE_ID, juce::String::fromUTF8 ("Fade I \xe2\x80\x93 II")),
      mix (s, P::DRY_WET_ID, "Mix"),
      output (s, P::OUTPUT_GAIN_ID, "Output", true),
      dynamic (s, P::COMP_DRY_WET_ID, "Dynamics"),
      threshold (s, P::COMP_THRESHOLD_ID, "Threshold"),
      ratio (s, P::COMP_RATIO_ID, "Ratio"),
      makeup (s, P::COMP_MAKEUPGAIN_ID, "Makeup")
{
    for (auto* r : { &input, &fade, &mix, &output, &dynamic, &threshold, &ratio, &makeup }) addAndMakeVisible (r);
}

void MasterSection::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    const int half = (r.getWidth() - 28) / 2;
    drawSectionTitle (g, r.withWidth (half).withHeight (34), "Master");
    drawSectionTitle (g, r.withTrimmedLeft (half + 28).withHeight (34), "Dynamics", "compressor");
}

void MasterSection::resized()
{
    auto r = getLocalBounds().withTrimmedTop (34);
    const int half = (r.getWidth() - 28) / 2;
    auto left = r.removeFromLeft (half);
    r.removeFromLeft (28);
    for (auto* row : { &input, &fade, &mix, &output })
    {
        row->setBounds (left.removeFromTop (ParamRow::preferredHeight));
        left.removeFromTop (2);
    }
    for (auto* row : { &dynamic, &threshold, &ratio, &makeup })
    {
        row->setBounds (r.removeFromTop (ParamRow::preferredHeight));
        r.removeFromTop (2);
    }
}

//==============================================================================
SphereView::SphereView (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& s) : proc (p), apvts (s)
{
    setTooltip ("Drag a source to set its base direction; the trail is the movement the latent adds.");
}

juce::Rectangle<float> SphereView::sphere() const
{
    auto b = getLocalBounds().toFloat().reduced (40.0f);
    const float size = juce::jmin (b.getWidth(), b.getHeight());
    return b.withSizeKeepingCentre (size, size);
}

juce::Point<float> SphereView::toScreen (float azimuthDegrees, float elevationDegrees) const
{
    const auto s = sphere();
    const float radius = s.getWidth() * 0.5f;
    const float az = juce::degreesToRadians (azimuthDegrees);
    const float horizontal = std::cos (juce::degreesToRadians (elevationDegrees));
    return { s.getCentreX() - radius * horizontal * std::sin (az), s.getCentreY() - radius * horizontal * std::cos (az) };
}

void SphereView::update()
{
    for (int n = 1; n <= 2; ++n)
    {
        auto& t = trails[(size_t) n - 1];
        t.positions[(size_t) t.writeIndex] = { proc.getAmbisonicAzimuth (n), proc.getAmbisonicElevation (n) };
        t.writeIndex = (t.writeIndex + 1) % trailLength;
        t.size = juce::jmin (t.size + 1, trailLength);
    }
    repaint();
}

void SphereView::paint (juce::Graphics& g)
{
    auto& A = Assets::get();
    const auto s = sphere();
    const auto c = s.getCentre();

    // engraved globe: horizon double ring, 30 and 60 degree rings, cross and 15 degree ticks
    g.setColour (col::panel);
    g.fillEllipse (s);
    g.setColour (col::ink.withAlpha (0.75f));
    g.drawEllipse (s, 1.4f);
    g.setColour (col::border);
    g.drawEllipse (s.expanded (4.0f), 0.6f);
    for (float elevation : { 30.0f, 60.0f })
    {
        const float r = s.getWidth() * 0.5f * std::cos (juce::degreesToRadians (elevation));
        g.drawEllipse (juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c), 0.7f);
    }
    g.drawLine (s.getX(), c.y, s.getRight(), c.y, 0.7f);
    g.drawLine (c.x, s.getY(), c.x, s.getBottom(), 0.7f);
    g.setColour (col::faint);
    for (int i = 0; i < 24; ++i)
    {
        const float a = (float) i / 24.0f * juce::MathConstants<float>::twoPi;
        const float r0 = s.getWidth() * 0.5f, r1 = r0 + (i % 6 == 0 ? 8.0f : 4.0f);
        g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), 0.8f);
    }

    g.setFont (A.italic (12.0f));
    g.setColour (col::mute);
    g.drawText ("front", juce::Rectangle<float> (60.0f, 16.0f).withCentre ({ c.x, s.getY() - 22.0f }), juce::Justification::centred);
    g.drawText ("back", juce::Rectangle<float> (60.0f, 16.0f).withCentre ({ c.x, s.getBottom() + 22.0f }), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ s.getX() - 18.0f, c.y }), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ s.getRight() + 18.0f, c.y }), juce::Justification::centred);

    const int order[2] { selected == 1 ? 2 : 1, selected };
    for (int n : order)
    {
        const bool on = networkOn (apvts, n);
        const float alpha = on ? 1.0f : 0.35f;
        const auto ink = (n == selected ? col::ink : col::mute).withMultipliedAlpha (alpha);

        // the trail of the latent movement, dotted like an engraving
        const auto& t = trails[(size_t) n - 1];
        for (int i = 1; i < t.size; i += 1)
        {
            const auto pos = t.positions[(size_t) ((t.writeIndex - i + 2 * trailLength) % trailLength)];
            const float fadeOut = 1.0f - (float) i / (float) trailLength;
            g.setColour (ink.withMultipliedAlpha (0.6f * fadeOut));
            g.fillEllipse (juce::Rectangle<float> (2.4f, 2.4f).withCentre (toScreen (pos.x, pos.y)));
        }

        // the base direction from the parameters: a small red ring for the selected source
        const auto ids = sourceIDs (n);
        const auto base = toScreen (apvts.getRawParameterValue (ids.azimuth.getParamID())->load(),
                                    apvts.getRawParameterValue (ids.elevation.getParamID())->load());
        g.setColour (n == selected ? col::rubric.withMultipliedAlpha (alpha) : col::faint.withMultipliedAlpha (alpha));
        g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (base), n == selected ? 1.4f : 0.9f);

        // the current direction, spread as a halo; below the horizon drawn hollow
        const float elevation = proc.getAmbisonicElevation (n);
        const auto pos = toScreen (proc.getAmbisonicAzimuth (n), elevation);
        const float halo = 16.0f + proc.getAmbisonicSpread (n) * s.getWidth() * 0.5f;
        g.setColour (ink.withMultipliedAlpha (0.12f));
        g.fillEllipse (juce::Rectangle<float> (halo, halo).withCentre (pos));
        drawNetworkMarker (g, elevation >= 0.0f ? n : 2, pos, 8.0f, ink);
        g.setFont (A.roman (14.0f));
        g.setColour (ink);
        g.drawText (networkNumeral (n), juce::Rectangle<float> (24.0f, 16.0f).withCentre (pos.translated (14.0f, -12.0f)), juce::Justification::centred);
    }
}

void SphereView::mouseDown (const juce::MouseEvent& e)
{
    float closest = 24.0f;
    dragged = 0;
    for (int n = 1; n <= 2; ++n)
    {
        const auto ids = sourceIDs (n);
        const auto base = toScreen (apvts.getRawParameterValue (ids.azimuth.getParamID())->load(),
                                    apvts.getRawParameterValue (ids.elevation.getParamID())->load());
        const auto now = toScreen (proc.getAmbisonicAzimuth (n), proc.getAmbisonicElevation (n));
        const float d = juce::jmin (base.getDistanceFrom (e.position), now.getDistanceFrom (e.position));
        if (d < closest) { closest = d; dragged = n; }
    }
    if (dragged == 0)
    {
        if (sphere().getCentre().getDistanceFrom (e.position) > sphere().getWidth() * 0.5f) return;
        dragged = selected;
    }
    if (dragged != selected && onSourceSelected) onSourceSelected (dragged);
    const auto ids = sourceIDs (dragged);
    apvts.getParameter (ids.azimuth.getParamID())->beginChangeGesture();
    apvts.getParameter (ids.elevation.getParamID())->beginChangeGesture();
    setBaseFromMouse (e.position);
}

void SphereView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged != 0) setBaseFromMouse (e.position);
}

void SphereView::mouseUp (const juce::MouseEvent&)
{
    if (dragged == 0) return;
    const auto ids = sourceIDs (dragged);
    apvts.getParameter (ids.azimuth.getParamID())->endChangeGesture();
    apvts.getParameter (ids.elevation.getParamID())->endChangeGesture();
    dragged = 0;
}

void SphereView::setBaseFromMouse (juce::Point<float> pos)
{
    const auto s = sphere();
    const auto offset = pos - s.getCentre();
    const float horizontal = juce::jlimit (0.0f, 1.0f, offset.getDistanceFromOrigin() / (s.getWidth() * 0.5f));
    const auto ids = sourceIDs (dragged);
    auto* az = apvts.getParameter (ids.azimuth.getParamID());
    auto* el = apvts.getParameter (ids.elevation.getParamID());
    // the view can't tell up from down, so the sign of the elevation is kept
    const float azimuth = juce::radiansToDegrees (std::atan2 (-offset.x, -offset.y));
    const float previous = apvts.getRawParameterValue (ids.elevation.getParamID())->load();
    const float elevation = std::copysign (juce::radiansToDegrees (std::acos (horizontal)), previous < 0.0f ? -1.0f : 1.0f);
    az->setValueNotifyingHost (az->convertTo0to1 (azimuth));
    el->setValueNotifyingHost (el->convertTo0to1 (elevation));
    repaint();
}

//==============================================================================
SpacePage::SourceControls::SourceControls (juce::AudioProcessorValueTreeState& s, int n)
{
    const auto ids = sourceIDs (n);
    azimuth = std::make_unique<ParamRow> (s, ids.azimuth, "Azimuth", true);
    elevation = std::make_unique<ParamRow> (s, ids.elevation, "Elevation", true);
    spread = std::make_unique<ParamRow> (s, ids.spread, "Spread");
    azimuthDepth = std::make_unique<ParamRow> (s, ids.azimuthDepth, "Azimuth depth");
    elevationDepth = std::make_unique<ParamRow> (s, ids.elevationDepth, "Elevation depth");
    spreadDepth = std::make_unique<ParamRow> (s, ids.spreadDepth, "Spread depth");
    const juce::ParameterID* latentIds[3] { &ids.azimuthLatent, &ids.elevationLatent, &ids.spreadLatent };
    for (int i = 0; i < 3; ++i)
    {
        latent[i].addItemList (PluginParameters::getLatentDimensionChoices(), 1);
        latentAttachment[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, latentIds[i]->getParamID(), latent[i]);
    }
}

juce::Array<juce::Component*> SpacePage::SourceControls::all()
{
    return { azimuth.get(), elevation.get(), spread.get(), azimuthDepth.get(), elevationDepth.get(), spreadDepth.get(),
             &latent[0], &latent[1], &latent[2] };
}

SpacePage::SpacePage (AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& s)
    : proc (p), apvts (s), sphereView (p, s), source1 (s, 1), source2 (s, 2),
      smoothing (s, P::AMBI_SMOOTHING_ID, "Smoothing"),
      dryWidth (s, P::AMBI_DRY_WIDTH_ID, "Dry width")
{
    addAndMakeVisible (sphereView);
    sphereView.onSourceSelected = [this] (int n) { selectSource (n); };
    for (int i = 0; i < 2; ++i)
    {
        sourceButton[i].setButtonText ("Source " + networkNumeral (i + 1));
        sourceButton[i].setClickingTogglesState (false);
        sourceButton[i].onClick = [this, i] { selectSource (i + 1); };
        addAndMakeVisible (sourceButton[i]);
    }
    for (auto* c : source1.all()) addChildComponent (c);
    for (auto* c : source2.all()) addChildComponent (c);
    addAndMakeVisible (smoothing);
    addAndMakeVisible (dryWidth);
    selectSource (1);
}

SpacePage::~SpacePage() { stopTimer(); }

void SpacePage::selectSource (int n)
{
    selected = n;
    for (auto* c : source1.all()) c->setVisible (n == 1);
    for (auto* c : source2.all()) c->setVisible (n == 2);
    sourceButton[0].setToggleState (n == 1, juce::dontSendNotification);
    sourceButton[1].setToggleState (n == 2, juce::dontSendNotification);
    sphereView.setSelectedSource (n);
    repaint();
}

void SpacePage::visibilityChanged()
{
    if (isVisible()) startTimerHz (30);
    else stopTimer();
}

void SpacePage::timerCallback()
{
    sphereView.update();
    repaint (latentBars.expanded (2));
    repaint (statusArea);
}

void SpacePage::resized()
{
    auto r = getLocalBounds().reduced (28, 16);
    sphereView.setBounds (r.removeFromLeft (560));
    r.removeFromLeft (40);

    auto right = r;
    auto buttons = right.removeFromTop (28);
    sourceButton[0].setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 6));
    buttons.removeFromLeft (12);
    sourceButton[1].setBounds (buttons);
    right.removeFromTop (14);

    const int half = (right.getWidth() - 28) / 2;
    auto columns = right.removeFromTop (34 + 3 * (ParamRow::preferredHeight + 4));
    sourceTitle = columns.withHeight (34);
    auto left = columns.withTrimmedTop (34).withWidth (half);
    auto rightCol = columns.withTrimmedTop (34).withTrimmedLeft (half + 28);
    for (auto* src : { &source1, &source2 })
    {
        auto l = left, rr = rightCol;
        for (auto* row : { src->azimuth.get(), src->elevation.get(), src->spread.get() })
        {
            row->setBounds (l.removeFromTop (ParamRow::preferredHeight));
            l.removeFromTop (4);
        }
        for (auto* row : { src->azimuthDepth.get(), src->elevationDepth.get(), src->spreadDepth.get() })
        {
            row->setBounds (rr.removeFromTop (ParamRow::preferredHeight));
            rr.removeFromTop (4);
        }
    }

    right.removeFromTop (12);
    latentTitle = right.removeFromTop (34);
    latentRows = right.removeFromTop (28);
    {
        auto boxes = latentRows;
        const int w = (boxes.getWidth() - 2 * 12) / 3;
        for (int i = 0; i < 3; ++i)
        {
            auto b = boxes.removeFromLeft (w).withTrimmedLeft (22);
            source1.latent[i].setBounds (b);
            source2.latent[i].setBounds (b);
            boxes.removeFromLeft (12);
        }
    }
    right.removeFromTop (12);
    latentBars = right.removeFromTop (70);
    statusArea = right.removeFromTop (40);

    right.removeFromTop (6);
    globalTitle = right.removeFromTop (34);
    auto g = right.removeFromTop (ParamRow::preferredHeight);
    smoothing.setBounds (g.removeFromLeft (half));
    g.removeFromLeft (28);
    dryWidth.setBounds (g);
}

void SpacePage::paint (juce::Graphics& g)
{
    auto& A = Assets::get();
    drawSectionTitle (g, sourceTitle, "Source " + networkNumeral (selected), juce::String::fromUTF8 ("base direction  \xc2\xb7  latent depth"));
    drawSectionTitle (g, latentTitle, "Latent mapping", "which dimension steers what");
    drawSectionTitle (g, globalTitle, "Field");

    // letters in front of the three latent boxes
    {
        auto boxes = latentRows;
        const int w = (boxes.getWidth() - 2 * 12) / 3;
        const char* letters[3] { "A", "E", "S" };
        g.setFont (A.roman (15.0f));
        g.setColour (col::rubric.brighter (0.25f));
        for (int i = 0; i < 3; ++i)
        {
            g.drawText (letters[i], boxes.removeFromLeft (w).withWidth (18), juce::Justification::centredLeft);
            boxes.removeFromLeft (12);
        }
    }

    // the live normalised latent of the selected source; mapped dimensions in ink with their letter
    const int count = ProcessorAmbisonics::numDisplayedLatentDimensions;
    const int numDims = juce::jmin (proc.getNumLatentDimensions (selected), count);
    const auto ids = sourceIDs (selected);
    const int mapped[3] { (int) apvts.getRawParameterValue (ids.azimuthLatent.getParamID())->load(),
                          (int) apvts.getRawParameterValue (ids.elevationLatent.getParamID())->load(),
                          (int) apvts.getRawParameterValue (ids.spreadLatent.getParamID())->load() };
    const char* letters[3] { "A", "E", "S" };
    const auto bars = latentBars.toFloat().withTrimmedBottom (14.0f);
    const float w = bars.getWidth() / (float) count;
    const float cy = bars.getCentreY();
    g.setColour (col::border);
    g.fillRect (bars.getX(), cy - 0.5f, bars.getWidth(), 1.0f);
    for (int d = 0; d < count; ++d)
    {
        const float x = bars.getX() + (float) d * w;
        int m = -1;
        for (int i = 0; i < 3; ++i) if (mapped[i] == d) m = i;
        if (d < numDims)
        {
            const float v = proc.getNormalisedLatent (selected, d);
            const float h = std::abs (v) * bars.getHeight() * 0.5f;
            g.setColour (m >= 0 ? col::ink : col::faint.withAlpha (0.6f));
            g.fillRect (juce::Rectangle<float> (x + 3.0f, v >= 0.0f ? cy - h : cy, w - 6.0f, juce::jmax (1.0f, h)));
        }
        if (m >= 0)
        {
            g.setColour (col::rubric.brighter (0.25f));
            g.setFont (A.roman (12.0f));
            g.drawText (letters[m], juce::Rectangle<float> (x, bars.getBottom() + 1.0f, w, 13.0f), juce::Justification::centred);
        }
    }

    juce::String status;
    const int order = proc.getAmbisonicOrder();
    if (order > 0)
    {
        const char* orderNames[4] { "", "first", "second", "third" };
        status = juce::String (orderNames[juce::jlimit (1, 3, order)]) + " order AmbiX, " + juce::String ((order + 1) * (order + 1)) + " channels";
    }
    else status = "stereo output, the spatial encoding is off";
    if (! networkOn (apvts, selected)) status += "\nsource " + networkNumeral (selected) + " is switched off";
    else if (proc.getNumLatentDimensions (selected) == 0) status += "\nno latent from this model yet";
    else status += "\n" + juce::String (proc.getNumLatentDimensions (selected)) + " latent dimensions";
    g.setFont (A.italic (12.0f));
    g.setColour (col::mute);
    g.drawFittedText (status, statusArea, juce::Justification::centredLeft, 2);
}
} // namespace scyclone::ui
