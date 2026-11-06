#pragma once

#include "PitchDetector.h"
#include "PluginProcessor.h"
#include "SpectrumView.h"
#include "Theme.h"

//==============================================================================
// Meter di livello peak con decay balistico. Riceve il picco (linear) via
// pushLevel() dal timer dell'editor; scala -60..0 dBFS, LED di clip a destra.
class LevelMeter : public juce::Component
{
public:
    void pushLevel (float linearPeak)
    {
        const float db = juce::Decibels::gainToDecibels (linearPeak, -60.0f);
        const float previousDb = displayDb;
        const bool  previousClip = clip;
        displayDb = juce::jmax (db, displayDb - 2.0f);   // decay ~60 dB/s @30fps

        // il LED si riaccende solo a un nuovo clip, poi resta ~1 s
        if (linearPeak >= 1.0f)
        {
            clip = true;
            clipHold = 30;                               // ~1 s @30fps
        }
        else if (clipHold > 0)
        {
            --clipHold;
        }
        else
        {
            clip = false;
        }

        if (! juce::approximatelyEqual (displayDb, previousDb) || clip != previousClip)
            repaint();   // a riposo il meter non ridisegna
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const float radius = r.getHeight() * 0.5f;
        auto bar = r.withTrimmedRight (10.0f);

        g.setColour (theme::raised);
        g.fillRoundedRectangle (bar, radius);

        const float norm = juce::jlimit (0.0f, 1.0f, (displayDb + 60.0f) / 60.0f);
        if (norm > 0.0f)
        {
            juce::ColourGradient grad (theme::low, bar.getX(), 0.0f, theme::clip, bar.getRight(), 0.0f, false);
            grad.addColour (0.7, theme::mid);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (r.getHeight(), bar.getWidth() * norm)), radius);
        }

        // tacche a -18 e -6 dB
        g.setColour (theme::background.withAlpha (0.6f));
        for (float mark : { -18.0f, -6.0f })
            g.fillRect (bar.getX() + bar.getWidth() * (mark + 60.0f) / 60.0f, bar.getY(), 1.0f, bar.getHeight());

        g.setColour (clip ? theme::clip : theme::raised);
        g.fillRoundedRectangle (r.withLeft (r.getRight() - 6.0f), 2.0f);
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

    // Legge i campioni e aggiorna nota, cent e descrizione accessibile
    // ("A2, +3 cent", vuota senza nota, "Tuner off"); il timer lo fa a 20 Hz.
    void update();

private:
    void timerCallback() override { update(); }
    void setShown (const juce::String& description);

    ParallaxStyleProcessor& processor;
    std::atomic<float>* pTunerOn = nullptr;

    static constexpr int windowSize = 2048;
    std::vector<float> window   = std::vector<float> (windowSize, 0.0f);
    std::vector<float> readTemp = std::vector<float> (windowSize, 0.0f);
    int windowFill = 0;

    float smoothedFreq = 0.0f;
    pitch::Note note;
    bool hasSignal = false;
};

//==============================================================================
// Manopola con etichetta sopra e valore sotto, in una cella di 64 x 80 px.
struct Knob
{
    static constexpr int width = 64, height = 80;

    juce::Slider slider;
    juce::Label  label;

    void setup (juce::Component& parent, const juce::String& name, juce::Colour colour,
                const juce::String& suffix = {});
    void setTopLeft (int x, int y);
};

//==============================================================================
// Card con titolo nel colore della sezione; quando la sezione è spenta il
// contenuto si oscura (i controlli restano usabili).
class Card : public juce::Component
{
public:
    Card (const juce::String& title, const juce::String& subtitle, juce::Colour colour, bool hasPowerSwitch);

    juce::ToggleButton power;
    std::function<bool()> isActive;                        // default: lo stato di power
    std::function<juce::Rectangle<int>()> dimmedArea;      // default: tutto sotto il titolo

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    static constexpr int headerHeight = 34;

private:
    juce::String title, subtitle;
    juce::Colour colour;
};

//==============================================================================
class BandsPage : public juce::Component
{
public:
    BandsPage();
    void resized() override;

    Card lowCard  { "LOW",  "COMP + SAT",   theme::low,  true };
    Card midCard  { "MID",  "EQ + DRIVE",   theme::mid,  true };
    Card highCard { "HIGH", "DRIVE + TONE", theme::high, true };

    Knob lowFreq, comp, lowSat, lowLevel;
    Knob midFrom, midTo, midGain, midDrive;
    Knob highFreq, drive, tone, highLevel;
    juce::ComboBox midCharacter, character;
    juce::Label midCharacterLabel, characterLabel;
};

class EqPage : public juce::Component
{
public:
    EqPage();
    void resized() override;

    Card loCard { "LOW SHELF",  {}, theme::accent, false };
    Card m1Card { "MID 1",      {}, theme::accent, false };
    Card m2Card { "MID 2",      {}, theme::accent, false };
    Card hiCard { "HIGH SHELF", {}, theme::accent, false };
    Knob loFreq, loGain, m1Freq, m1Gain, m2Freq, m2Gain, hiFreq, hiGain;
};

class CabPage : public juce::Component
{
public:
    CabPage();
    void resized() override;

    Card card { "CAB IR", "IMPULSE RESPONSE", theme::accent, false };
    juce::ComboBox position;
    juce::Label positionLabel;
    juce::TextButton loadIR { "LOAD IR" };
    juce::Label irName;
    Knob irLevel;
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

public:
    // Meter e nome dell'IR; il timer lo fa a 30 Hz
    void refreshIndicators();

private:
    void timerCallback() override { refreshIndicators(); }
    void setupHeader();       // titolo, versione, preset, reset
    void setupPages();        // BANDS, EQ, CAB
    void setupFooter();       // ingresso, meter, tuner, uscita
    void attachParameters();

    ParallaxStyleProcessor& processor;
    ParallaxLookAndFeel lnf;

    // Header
    juce::Label titleLabel, versionLabel;
    juce::TextButton presetSaveB { "SAVE" }, presetLoadB { "LOAD" }, resetB { "RESET" };

    // Spettro con le bande
    SpectrumView spectrum;

    // Pagine
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    BandsPage bandsPage;
    EqPage eqPage;
    CabPage cabPage;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Footer: ingresso, meter, tuner, uscita
    Knob input, gate, blend, output;
    LevelMeter inMeter, outMeter;
    juce::Label inMeterLabel, outMeterLabel;
    juce::ToggleButton tunerToggle { "TUNER" };
    TunerComponent tuner;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>>  comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParallaxStyleEditor)
};
