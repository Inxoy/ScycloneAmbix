#pragma once

// The look of the rysum plugin: black and white like an old print, IM Fell type,
// ruled double lines, lozenge thumbs and one rubric red for initials.

#include "JuceHeader.h"

namespace scyclone::ui
{
namespace col
{
    const juce::Colour bg     { 0xff0d0d0c };
    const juce::Colour ink    { 0xffe9e7e2 };
    const juce::Colour white  { 0xffffffff };
    const juce::Colour dim    { 0xffb9b7b2 };
    const juce::Colour mute   { 0xffa3a19b };
    const juce::Colour faint  { 0xff858380 };
    const juce::Colour line   { 0xff2a2a28 };
    const juce::Colour border { 0xff3f3f3c };
    const juce::Colour track  { 0xff34342f };
    const juce::Colour panel  { 0xff141413 };
    const juce::Colour rubric { 0xffa8442e };   // the red of rubricated initials
}

/** fonts and the grain tile, created once per process */
struct Assets
{
    juce::Typeface::Ptr fell, fellItalic;
    juce::Image grainTile;

    static Assets& get()
    {
        static Assets assets;
        return assets;
    }

    // Fell has a small x-height: small sizes are set a little larger so they stay readable
    static float readable (float h, float k) { return (h + juce::jmax (0.0f, 14.0f - h) * 0.45f + 0.5f) * k; }
    juce::Font roman (float h) const  { return juce::Font (fell).withHeight (readable (h, 1.16f)); }
    juce::Font italic (float h) const { return juce::Font (fellItalic).withHeight (readable (h, 1.2f)); }
    juce::Font title (float h) const  { return juce::Font (fell).withHeight (h); }

private:
    Assets()
    {
        fell       = juce::Typeface::createSystemTypefaceFor (BinaryData::RysumFellRoman_ttf, BinaryData::RysumFellRoman_ttfSize);
        fellItalic = juce::Typeface::createSystemTypefaceFor (BinaryData::RysumFellItalic_ttf, BinaryData::RysumFellItalic_ttfSize);
        grainTile  = juce::ImageCache::getFromMemory (BinaryData::grain_png, BinaryData::grain_pngSize);
    }
};

inline void drawGrain (juce::Graphics& g, juce::Rectangle<int> r, float opacity)
{
    auto& t = Assets::get().grainTile;
    if (! t.isValid()) return;
    g.setTiledImageFill (t, 0, 0, opacity);
    g.fillRect (r);
}

/** a small printer's lozenge */
inline void fillLozenge (juce::Graphics& g, juce::Point<float> c, float rw, float rh)
{
    juce::Path p;
    p.startNewSubPath (c.x, c.y - rh); p.lineTo (c.x + rw, c.y); p.lineTo (c.x, c.y + rh); p.lineTo (c.x - rw, c.y); p.closeSubPath();
    g.fillPath (p);
}

/** section head: thick-thin double rule ending in a red lozenge, the title in spaced
    capitals with a red initial, an optional note on the right */
inline void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& right = {})
{
    auto& A = Assets::get();
    const float x0 = (float) r.getX(), x1 = (float) r.getRight(), y = (float) r.getY();
    g.setColour (col::ink.withAlpha (0.9f));
    g.fillRect (x0, y, x1 - x0, 1.5f);
    g.setColour (col::ink.withAlpha (0.45f));
    g.fillRect (x0, y + 3.5f, x1 - x0, 0.6f);
    g.setColour (col::rubric);
    fillLozenge (g, { x1 - 2.0f, y + 2.0f }, 3.0f, 4.5f);

    auto line = r.withTrimmedTop (8).withHeight (22);
    const auto upper = title.toUpperCase();
    if (upper.isNotEmpty())
    {
        const auto initial = upper.substring (0, 1), rest = upper.substring (1);
        const auto big = A.roman (19.0f), small = A.roman (14.5f).withExtraKerningFactor (0.14f);
        g.setColour (col::rubric.brighter (0.25f));
        g.setFont (big);
        const int iw = juce::roundToInt (big.getStringWidthFloat (initial)) + 2;
        g.drawText (initial, line.withWidth (iw + 4), juce::Justification::centredLeft);
        g.setColour (col::ink);
        g.setFont (small);
        g.drawText (rest, line.withTrimmedLeft (iw).translated (0, 1), juce::Justification::centredLeft);
    }
    if (right.isNotEmpty())
    {
        g.setColour (col::mute);
        g.setFont (A.italic (11.0f));
        g.drawText (right, line, juce::Justification::centredRight);
    }
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::Label::textColourId, col::ink);
        setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonOnColourId, col::ink);
        setColour (juce::TextButton::textColourOffId, col::dim);
        setColour (juce::TextButton::textColourOnId, col::bg);
        setColour (juce::ComboBox::outlineColourId, col::border);
        setColour (juce::ComboBox::backgroundColourId, col::bg);
        setColour (juce::ComboBox::textColourId, col::ink);
        setColour (juce::ComboBox::arrowColourId, col::dim);
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::headerTextColourId, col::mute);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::ink);
        setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
        setColour (juce::TooltipWindow::backgroundColourId, col::panel);
        setColour (juce::TooltipWindow::textColourId, col::ink);
        setColour (juce::TooltipWindow::outlineColourId, col::border);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::ink);
        setColour (juce::AlertWindow::outlineColourId, col::border);
        setColour (juce::TextEditor::backgroundColourId, col::bg);
        setColour (juce::TextEditor::textColourId, col::ink);
        setColour (juce::TextEditor::outlineColourId, col::border);
        setColour (juce::TextEditor::focusedOutlineColourId, col::dim);
        setColour (juce::TextEditor::highlightColourId, col::faint);
        setColour (juce::CaretComponent::caretColourId, col::ink);
        setColour (juce::ScrollBar::thumbColourId, col::faint);
        setDefaultSansSerifTypeface (Assets::get().fell);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        // a double rule like a ruled frame in an old book; on = parchment
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        if (b.getToggleState())
        {
            g.setColour (col::ink);
            g.fillRect (r);
            g.setColour (col::bg.withAlpha (0.35f));
            g.drawRect (r.reduced (2.0f), 0.6f);
        }
        else
        {
            g.setColour (over || down ? col::dim : col::border);
            g.drawRect (r, 1.0f);
            g.setColour ((over || down ? col::dim : col::border).withAlpha (0.45f));
            g.drawRect (r.reduced (2.5f), 0.6f);
        }
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return Assets::get().roman (13.0f); }
    juce::Font getPopupMenuFont() override { return Assets::get().roman (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Assets::get().roman (juce::jmin (13.5f, (float) h * 0.56f)); }
    juce::Font getLabelFont (juce::Label&) override { return Assets::get().roman (13.0f); }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.getToggleState() ? col::bg : (b.isEnabled() ? col::dim : col::faint));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::bg.withAlpha (0.6f));
        g.fillRect (r);
        g.setColour (box.isMouseOver (true) ? col::dim : col::border);
        g.drawRect (r, 1.0f);
        g.setColour (col::border.withAlpha (0.45f));
        g.drawRect (r.reduced (2.5f), 0.6f);
        const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
        juce::Path p;
        p.startNewSubPath (cx - 4.0f, cy - 2.0f); p.lineTo (cx, cy + 2.5f); p.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (box.isEnabled() ? col::dim : col::faint);
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (col::panel);
        g.setColour (col::border);
        g.drawRect (0, 0, w, h, 1);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (col::panel);
        g.setColour (col::border);
        g.drawRect (0, 0, w, h, 1);
        g.setColour (col::ink);
        g.setFont (Assets::get().roman (13.0f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (6, 3), juce::Justification::centredLeft, 4);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        // a ruled line with tick marks at the ends and a lozenge thumb
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (col::track);
        g.fillRect ((float) x, cy - 0.5f, (float) w, 1.0f);
        g.setColour (col::ink.withAlpha (s.isEnabled() ? 0.85f : 0.35f));
        if (s.getProperties().contains ("bipolar"))
        {
            const float mid = (float) x + (float) w * 0.5f;
            g.fillRect (juce::jmin (mid, pos), cy - 1.0f, std::abs (pos - mid), 2.0f);
            g.setColour (col::faint);
            g.fillRect (mid - 0.5f, cy - 4.0f, 1.0f, 8.0f);
        }
        else g.fillRect ((float) x, cy - 1.0f, pos - (float) x, 2.0f);
        g.setColour (col::faint);
        g.fillRect ((float) x, cy - 3.0f, 1.0f, 6.0f);
        g.fillRect ((float) (x + w) - 1.0f, cy - 3.0f, 1.0f, 6.0f);

        juce::Path p;
        p.startNewSubPath (pos, cy - 7.0f); p.lineTo (pos + 4.5f, cy); p.lineTo (pos, cy + 7.0f); p.lineTo (pos - 4.5f, cy); p.closeSubPath();
        g.setColour (! s.isEnabled() ? col::faint : s.isMouseOverOrDragging() ? col::white : col::ink);
        g.fillPath (p);
        g.setColour (col::bg);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    int getSliderThumbRadius (juce::Slider&) override { return 5; }
};

/** a one-line text field over a value: Return applies, Escape or clicking elsewhere cancels */
class InlineValueEditor
{
public:
    void open (juce::Component& parent, juce::Rectangle<int> area, juce::RangedAudioParameter* p)
    {
        if (p == nullptr) return;
        param = p;
        ed = std::make_unique<juce::TextEditor>();
        ed->setFont (Assets::get().italic (12.0f));
        ed->setJustification (juce::Justification::centredRight);
        ed->setText (p->getCurrentValueAsText(), false);
        ed->setBounds (area);
        ed->setIndents (3, 1);
        parent.addAndMakeVisible (*ed);
        ed->selectAll();
        ed->grabKeyboardFocus();
        ed->onReturnKey = [this]
        {
            // the parameters parse their own units ("300 ms", "-6 dB", "50" on a % value)
            const float v = param->getValueForText (ed->getText());
            param->beginChangeGesture();
            param->setValueNotifyingHost (v);
            param->endChangeGesture();
            close();
        };
        ed->onEscapeKey = [this] { close(); };
        ed->onFocusLost = [this] { close(); };
    }

private:
    void close()
    {
        // deferred: we may be inside one of the editor's own callbacks
        if (ed == nullptr) return;
        auto* e = ed.release();
        e->onReturnKey = nullptr; e->onEscapeKey = nullptr; e->onFocusLost = nullptr;
        e->setVisible (false);
        juce::MessageManager::callAsync ([e] { delete e; });
    }
    std::unique_ptr<juce::TextEditor> ed;
    juce::RangedAudioParameter* param = nullptr;
};

/** label + value + thin slider, attached to a parameter. Click the value to type one,
    double-click the slider for the default. */
class ParamRow : public juce::Component
{
public:
    ParamRow (juce::AudioProcessorValueTreeState& state, const juce::ParameterID& paramId, const juce::String& name, bool bipolar = false)
        : title (name)
    {
        param = state.getParameter (paramId.getParamID());
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        if (bipolar) slider.getProperties().set ("bipolar", true);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId.getParamID(), slider);
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
        slider.onValueChange = [this] { repaint (headerArea()); };
        slider.setTooltip (param->getName (64));
    }

    void paint (juce::Graphics& g) override
    {
        auto& A = Assets::get();
        auto r = headerArea();
        g.setColour (isEnabled() ? col::ink : col::faint);
        g.setFont (A.roman (14.0f));
        g.drawText (title, r, juce::Justification::centredLeft);
        g.setFont (A.italic (12.0f));
        g.setColour (valueHover ? col::white : col::dim);
        g.drawText (param->getCurrentValueAsText() + suffix(), r, juce::Justification::centredRight);
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (16).withHeight (18)); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (valueArea().contains (e.getPosition()))
            editor.open (*this, valueArea().expanded (2, 1).withTrimmedLeft (-30), param);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool h = valueArea().contains (e.getPosition());
        if (h != valueHover) { valueHover = h; setMouseCursor (h ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor); repaint (headerArea()); }
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        if (valueHover) { valueHover = false; setMouseCursor (juce::MouseCursor::NormalCursor); repaint (headerArea()); }
    }

    static constexpr int preferredHeight = 38;

private:
    juce::String suffix() const
    {
        const auto label = param->getLabel();
        return label.isEmpty() ? juce::String() : " " + label;
    }
    juce::Rectangle<int> headerArea() const { return getLocalBounds().removeFromTop (18); }
    juce::Rectangle<int> valueArea() const { return headerArea().removeFromRight (96); }

    juce::String title;
    juce::RangedAudioParameter* param = nullptr;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    InlineValueEditor editor;
    bool valueHover = false;
};

/** a ruled toggle bound to a bool parameter */
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& state, const juce::ParameterID& paramId, const juce::String& text)
        : juce::TextButton (text)
    {
        setClickingTogglesState (true);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId.getParamID(), *this);
    }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
} // namespace scyclone::ui
