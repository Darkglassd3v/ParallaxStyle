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
// Accordatore: legge i campioni decimati dal FIFO del processor,
// stima il pitch con YIN (difference function + CMND + interp. parabolica).
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

    static constexpr int windowSize = 2048;   // @12 kHz = ~170 ms, fino a ~12 Hz
    std::vector<float> window   = std::vector<float> (windowSize, 0.0f);
    std::vector<float> readTemp = std::vector<float> (windowSize, 0.0f);
    int windowFill = 0;

    float smoothedFreq = 0.0f;
    juce::String noteName = "-";
    float cents = 0.0f;
    bool hasSignal = false;
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

    void setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name);
    void timerCallback() override;   // aggiorna la label del file IR

    ParallaxStyleProcessor& processor;
    DarkLookAndFeel lnf;

    juce::Slider xoverS, compS, lowSatS, lowLevelS, driveS, toneS, highLevelS, blendS, outputS;
    juce::Label  xoverL, compL, lowSatL, lowLevelL, driveL, toneL, highLevelL, blendL, outputL;
    juce::ComboBox characterBox, cabBox;
    juce::Label characterL, cabL;

    juce::TextButton loadIRButton { "LOAD IR" };
    juce::Label irNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    TunerComponent tuner;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> characterAttachment, cabAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParallaxStyleEditor)
};
