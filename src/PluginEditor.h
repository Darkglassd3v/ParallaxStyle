#pragma once

#include "PluginProcessor.h"

//==============================================================================
class DarkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DarkLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,    juce::Colour (0xffd0d0d0));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId,   juce::Colour (0xff1a1a1e));
        setColour (juce::ComboBox::textColourId,         juce::Colour (0xffd0d0d0));
        setColour (juce::ComboBox::outlineColourId,      juce::Colour (0xff3a3a42));
        setColour (juce::PopupMenu::backgroundColourId,  juce::Colour (0xff1a1a1e));
        setColour (juce::PopupMenu::textColourId,        juce::Colour (0xffd0d0d0));
        setColour (juce::TextButton::buttonColourId,     juce::Colour (0xff26262c));
        setColour (juce::TextButton::textColourOffId,    juce::Colour (0xffd0d0d0));
        setColour (juce::ToggleButton::textColourId,     juce::Colour (0xffd0d0d0));
        setColour (juce::ToggleButton::tickColourId,     juce::Colour (0xff00c8ff));
        setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TabbedComponent::outlineColourId,    juce::Colour (0xff34343c));
        setColour (juce::TabbedButtonBar::tabTextColourId,         juce::Colour (0xff909098));
        setColour (juce::TabbedButtonBar::frontTextColourId,       juce::Colour (0xff00c8ff));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                              (float) width, (float) height).reduced (6.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre  = bounds.getCentre();
        const float angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float lineW  = radius * 0.12f;
        const float arcR   = radius - lineW * 0.5f;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff2a2a30));
        g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (juce::Colour (0xff00c8ff));
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        const float knobR = arcR - lineW * 1.2f;
        g.setColour (juce::Colour (0xff18181c));
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (juce::Colour (0xff3a3a42));
        g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.5f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-lineW * 0.4f, -knobR, lineW * 0.8f, knobR * 0.45f, lineW * 0.3f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                    .translated (centre.x, centre.y));
        g.setColour (juce::Colour (0xffe8e8e8));
        g.fillPath (pointer);
    }
};

//==============================================================================
// Meter di livello peak con decay balistico. Riceve il picco (linear) via
// pushLevel() dal timer dell'editor; scala -60..0 dBFS.
class LevelMeter : public juce::Component
{
public:
    void pushLevel (float linearPeak)
    {
        const float db = juce::Decibels::gainToDecibels (linearPeak, -60.0f);
        displayDb = juce::jmax (db, displayDb - 2.0f);   // decay ~60 dB/s @30fps
        clip = clip || linearPeak >= 1.0f;
        if (db >= displayDb) clipHold = 30;              // ~1 s
        if (clipHold > 0) --clipHold; else clip = false;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff1a1a1e));
        g.fillRoundedRectangle (r, 3.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, (displayDb + 60.0f) / 60.0f);
        auto fill = r.reduced (2.0f);
        fill.setWidth (fill.getWidth() * norm);

        juce::ColourGradient grad (juce::Colour (0xff40e080), r.getX(), 0.0f,
                                   juce::Colour (0xffe04040), r.getRight(), 0.0f, false);
        grad.addColour (0.75, juce::Colour (0xffe0c040));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 2.0f);

        if (clip)
        {
            g.setColour (juce::Colour (0xffe04040));
            g.fillRect (r.getRight() - 6.0f, r.getY(), 4.0f, r.getHeight());
        }

        // tacche a -18 e -6 dB
        g.setColour (juce::Colour (0x50ffffff));
        for (float mark : { -18.0f, -6.0f })
        {
            const float mx = r.getX() + r.getWidth() * (mark + 60.0f) / 60.0f;
            g.fillRect (mx, r.getY(), 1.0f, r.getHeight());
        }
    }

private:
    float displayDb = -60.0f;
    bool clip = false;
    int clipHold = 0;
};

//==============================================================================
class TunerComponent : public juce::Component,
                       private juce::Timer
{
public:
    explicit TunerComponent (ParallaxStyleProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float detectPitch() const;

    ParallaxStyleProcessor& processor;
    std::atomic<float>* pTunerOn = nullptr;

    static constexpr int windowSize = 2048;
    std::vector<float> window   = std::vector<float> (windowSize, 0.0f);
    std::vector<float> readTemp = std::vector<float> (windowSize, 0.0f);
    int windowFill = 0;

    float smoothedFreq = 0.0f;
    juce::String noteName = "-";
    float cents = 0.0f;
    bool hasSignal = false;
};

//==============================================================================
// Pagina BANDS del tab
class BandsPage : public juce::Component
{
public:
    juce::Slider compS, lowSatS, xoverS, lowLevelS, driveS, toneS, highLevelS;
    juce::Label  compL, lowSatL, xoverL, lowLevelL, driveL, toneL, highLevelL;
    juce::ComboBox characterBox;
    juce::Label characterL;

    BandsPage();
    void paint (juce::Graphics&) override;
    void resized() override;
};

//==============================================================================
// Pagina CAB IR del tab
class CabPage : public juce::Component
{
public:
    juce::ComboBox cabBox;
    juce::Label cabL;
    juce::TextButton loadIRButton { "LOAD IR" };
    juce::Label irNameLabel;

    CabPage();
    void resized() override;
};

//==============================================================================
class ParallaxStyleEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit ParallaxStyleEditor (ParallaxStyleProcessor&);
    ~ParallaxStyleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;   // meter + label IR

    ParallaxStyleProcessor& processor;
    DarkLookAndFeel lnf;

    // Top bar (sempre visibile)
    juce::Slider inputS, gateS, blendS, outputS;
    juce::Label  inputL, gateL, blendL, outputL;
    LevelMeter inMeter, outMeter;
    juce::Label inMeterL, outMeterL;

    // Tab centrale
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    BandsPage bandsPage;
    CabPage cabPage;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Tuner (sempre visibile, abilitabile)
    juce::ToggleButton tunerToggle { "TUNER" };
    TunerComponent tuner;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> characterAttachment, cabAttachment;
    std::unique_ptr<ButtonAttachment> tunerAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParallaxStyleEditor)
};
