#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Palette e stile della grafica: scuro, piatto, un colore per banda.
// Testo principale e secondario hanno un contrasto di almeno 4.5:1 sullo sfondo.
namespace theme
{
    inline const juce::Colour background { 0xff0b0c10 };
    inline const juce::Colour surface    { 0xff12141a };   // pannelli
    inline const juce::Colour raised     { 0xff191c24 };   // card e manopole
    inline const juce::Colour border     { 0xff262a35 };
    inline const juce::Colour borderHi   { 0xff3a3f4d };
    inline const juce::Colour text       { 0xffe8eaf0 };   // 16:1 sullo sfondo
    inline const juce::Colour muted      { 0xff8d93a5 };   // 6.4:1 sullo sfondo
    inline const juce::Colour accent     { 0xff7aa2ff };   // controlli globali, EQ
    inline const juce::Colour low        { 0xff3ddc97 };
    inline const juce::Colour mid        { 0xffffb547 };
    inline const juce::Colour high       { 0xffff5c8a };
    inline const juce::Colour clip       { 0xffff4d4f };

    inline juce::FontOptions font (float size, bool bold = false)
    {
        return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
    }

    // Etichette in maiuscolo con un po' di spaziatura
    inline juce::FontOptions caption()  { return font (10.5f, true).withKerningFactor (0.06f); }
}

//==============================================================================
class ParallaxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ParallaxLookAndFeel()
    {
        using namespace theme;
        setColour (juce::ResizableWindow::backgroundColourId,  background);
        setColour (juce::Slider::textBoxTextColourId,          muted);
        setColour (juce::Slider::textBoxOutlineColourId,       juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,     accent.withAlpha (0.4f));
        setColour (juce::Slider::rotarySliderFillColourId,     accent);
        setColour (juce::Slider::rotarySliderOutlineColourId,  border);
        setColour (juce::Label::textColourId,                  muted);
        setColour (juce::ComboBox::backgroundColourId,         raised);
        setColour (juce::ComboBox::textColourId,               text);
        setColour (juce::ComboBox::outlineColourId,            border);
        setColour (juce::ComboBox::arrowColourId,              muted);
        setColour (juce::ComboBox::focusedOutlineColourId,     accent);
        setColour (juce::PopupMenu::backgroundColourId,        raised);
        setColour (juce::PopupMenu::textColourId,              text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, border);
        setColour (juce::PopupMenu::highlightedTextColourId,   text);
        setColour (juce::TextButton::buttonColourId,           juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonOnColourId,         border);
        setColour (juce::TextButton::textColourOffId,          muted);
        setColour (juce::TextButton::textColourOnId,           text);
        setColour (juce::ToggleButton::textColourId,           muted);
        setColour (juce::ToggleButton::tickColourId,           accent);
        setColour (juce::TabbedComponent::backgroundColourId,  juce::Colours::transparentBlack);
        setColour (juce::TabbedComponent::outlineColourId,     juce::Colours::transparentBlack);
        setColour (juce::TabbedButtonBar::tabTextColourId,     muted);
        setColour (juce::TabbedButtonBar::frontTextColourId,   text);
        setColour (juce::TabbedButtonBar::tabOutlineColourId,  juce::Colours::transparentBlack);
        setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
    }

    // Manopola: anello sottile, arco del valore nel colore della sezione, corpo piatto
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  centre = bounds.getCentre();
        const float angle  = startAngle + sliderPos * (endAngle - startAngle);
        const float lineW  = juce::jmax (2.5f, radius * 0.11f);
        const float arcR   = radius - lineW * 0.5f;
        const bool  hover  = slider.isMouseOverOrDragging() && slider.isEnabled();
        const auto  fill   = slider.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (theme::border);
        g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // archi bipolari (gain in dB) partono dal centro, gli altri dall'inizio
        const bool bipolar  = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float fromAng = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                      : startAngle;
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (fromAng, angle), juce::jmax (fromAng, angle), true);
        g.setColour (hover ? fill.brighter (0.25f) : fill);
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float bodyR = arcR - lineW * 1.6f;
        g.setColour (theme::raised);
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (hover ? theme::borderHi : theme::border);
        g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.25f, -bodyR + 3.0f, 2.5f, bodyR * 0.5f, 1.25f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.setColour (theme::text);
        g.fillPath (pointer);

        if (slider.hasKeyboardFocus (true))
        {
            g.setColour (theme::accent);
            g.drawEllipse (bounds.reduced (1.0f), 1.5f);
        }
    }

    // Interruttore a pillola (accensione delle bande, tuner)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) override
    {
        const auto area   = button.getLocalBounds().toFloat();
        const bool on     = button.getToggleState();
        const auto colour = button.findColour (juce::ToggleButton::tickColourId);
        const auto pill   = area.withWidth (34.0f).withSizeKeepingCentre (34.0f, 18.0f).withX (area.getX() + 1.0f);

        g.setColour (on ? colour : theme::border);
        g.fillRoundedRectangle (pill, 9.0f);
        const float knobX = on ? pill.getRight() - 16.0f : pill.getX() + 2.0f;
        g.setColour (on ? theme::background : (highlighted ? theme::text : theme::muted));
        g.fillEllipse (knobX, pill.getY() + 2.0f, 14.0f, 14.0f);

        if (button.hasKeyboardFocus (true))
        {
            g.setColour (theme::accent);
            g.drawRoundedRectangle (pill.expanded (2.0f), 11.0f, 1.5f);
        }

        if (button.getButtonText().isNotEmpty())
        {
            g.setColour (on ? theme::text : theme::muted);
            g.setFont (theme::caption());
            g.drawText (button.getButtonText(), area.withLeft (pill.getRight() + 8.0f), juce::Justification::centredLeft);
        }
    }

    // Bottoni "ghost": solo bordo, si accendono al passaggio del mouse
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? theme::border : (highlighted ? theme::raised : juce::Colours::transparentBlack));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (highlighted || button.hasKeyboardFocus (true) ? theme::borderHi : theme::border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::caption(); }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down) override
    {
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.setColour (highlighted || down ? theme::text : theme::muted);
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (theme::raised);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.isMouseOver (true) || box.hasKeyboardFocus (true) ? theme::borderHi : theme::border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        juce::Path chevron;
        const float cx = (float) width - 14.0f, cy = (float) height * 0.5f;
        chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (theme::muted);
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (12.5f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getPopupMenuFont() override { return theme::font (13.0f); }

    // Tab: solo testo, quella attiva sottolineata
    int getTabButtonBestWidth (juce::TabBarButton& button, int) override
    {
        return (int) juce::GlyphArrangement::getStringWidth (theme::caption(), button.getButtonText()) + 28;
    }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool highlighted, bool) override
    {
        const bool front = button.isFrontTab();
        auto area = button.getLocalBounds().toFloat();
        g.setFont (theme::caption());
        g.setColour (front ? theme::text : (highlighted ? theme::text.withAlpha (0.8f) : theme::muted));
        g.drawText (button.getButtonText(), area.withTrimmedBottom (3.0f), juce::Justification::centred);
        if (front)
        {
            g.setColour (theme::accent);
            g.fillRoundedRectangle (area.removeFromBottom (2.0f).reduced (10.0f, 0.0f), 1.0f);
        }
    }

    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar& bar, juce::Graphics& g, int w, int h) override
    {
        juce::ignoreUnused (bar);
        g.setColour (theme::border);
        g.fillRect (0, h - 1, w, 1);
    }

    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }

    // Valore sotto la manopola: testo piccolo, senza riquadro
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (slider);
        l->setFont (theme::font (11.5f));
        l->setColour (juce::Label::textColourId, theme::muted);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineWhenEditingColourId, theme::accent);
        l->setColour (juce::TextEditor::textColourId, theme::text);
        l->setColour (juce::TextEditor::backgroundColourId, theme::raised);
        l->setColour (juce::TextEditor::highlightColourId, theme::accent.withAlpha (0.4f));
        return l;
    }
};
