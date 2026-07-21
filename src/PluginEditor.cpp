#include "PluginEditor.h"

//==============================================================================
// Helpers locali
//==============================================================================
static void styleKnob (juce::Component& parent, juce::Slider& s, juce::Label& l,
                       const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    parent.addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, juce::Colour (0xff909098));
    parent.addAndMakeVisible (l);
}

//==============================================================================
// TunerComponent
//==============================================================================
TunerComponent::TunerComponent (ParallaxStyleProcessor& p) : processor (p)
{
    pTunerOn = processor.apvts.getRawParameterValue ("tuneron");
    startTimerHz (20);
}

void TunerComponent::timerCallback()
{
    if (pTunerOn->load() < 0.5f)
    {
        if (hasSignal || windowFill > 0)
        {
            hasSignal = false;
            windowFill = 0;
            smoothedFreq = 0.0f;
        }
        repaint();
        return;
    }

    const int got = processor.readTunerSamples (readTemp.data(), (int) readTemp.size());
    for (int i = 0; i < got; ++i)
    {
        if (windowFill < windowSize)
        {
            window[(size_t) windowFill++] = readTemp[(size_t) i];
        }
        else
        {
            std::move (window.begin() + 1, window.end(), window.begin());
            window[(size_t) windowSize - 1] = readTemp[(size_t) i];
        }
    }

    if (windowFill < windowSize)
        return;

    float rms = 0.0f;
    for (int i = 0; i < windowSize; ++i)
        rms += window[(size_t) i] * window[(size_t) i];
    rms = std::sqrt (rms / (float) windowSize);

    if (rms < 0.003f)
    {
        hasSignal = false;
        smoothedFreq = 0.0f;
        repaint();
        return;
    }

    const float freq = detectPitch();
    if (freq > 0.0f)
    {
        smoothedFreq = smoothedFreq <= 0.0f ? freq
                     : 0.7f * smoothedFreq + 0.3f * freq;

        const float midi = 69.0f + 12.0f * std::log2 (smoothedFreq / 440.0f);
        const int   nearest = (int) std::lround (midi);
        cents = (midi - (float) nearest) * 100.0f;

        static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };
        const int octave = nearest / 12 - 1;
        noteName = juce::String (names[((nearest % 12) + 12) % 12]) + juce::String (octave);
        hasSignal = true;
    }

    repaint();
}

float TunerComponent::detectPitch() const
{
    const double sr = processor.getTunerSampleRate();
    const int maxTau = windowSize / 2;
    const int minTau = juce::jmax (2, (int) (sr / 400.0));

    std::vector<float> diff ((size_t) maxTau, 0.0f);

    for (int tau = minTau; tau < maxTau; ++tau)
    {
        float sum = 0.0f;
        for (int i = 0; i < maxTau; ++i)
        {
            const float d = window[(size_t) i] - window[(size_t) (i + tau)];
            sum += d * d;
        }
        diff[(size_t) tau] = sum;
    }

    std::vector<float> cmnd ((size_t) maxTau, 1.0f);
    float runningSum = 0.0f;
    for (int tau = minTau; tau < maxTau; ++tau)
    {
        runningSum += diff[(size_t) tau];
        cmnd[(size_t) tau] = runningSum > 0.0f
                           ? diff[(size_t) tau] * (float) (tau - minTau + 1) / runningSum
                           : 1.0f;
    }

    constexpr float threshold = 0.15f;
    int tauEstimate = -1;
    for (int tau = minTau + 1; tau < maxTau - 1; ++tau)
    {
        if (cmnd[(size_t) tau] < threshold)
        {
            while (tau + 1 < maxTau - 1 && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                ++tau;
            tauEstimate = tau;
            break;
        }
    }

    if (tauEstimate < 0)
        return -1.0f;

    const float y0 = cmnd[(size_t) (tauEstimate - 1)];
    const float y1 = cmnd[(size_t) tauEstimate];
    const float y2 = cmnd[(size_t) (tauEstimate + 1)];
    const float denom = 2.0f * (y0 - 2.0f * y1 + y2);
    const float delta = std::abs (denom) > 1.0e-12f ? (y0 - y2) / denom : 0.0f;

    return (float) (sr / ((double) tauEstimate + (double) delta));
}

void TunerComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if (pTunerOn->load() < 0.5f)
    {
        g.setColour (juce::Colour (0xff505058));
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText ("TUNER OFF", bounds, juce::Justification::centred);
        return;
    }

    if (! hasSignal)
    {
        g.setColour (juce::Colour (0xff505058));
        g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
        g.drawText ("-", bounds.removeFromTop (52), juce::Justification::centred);
        return;
    }

    const bool inTune = std::abs (cents) < 5.0f;

    g.setColour (inTune ? juce::Colour (0xff40e080) : juce::Colour (0xffe8e8e8));
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    g.drawText (noteName, bounds.removeFromTop (46), juce::Justification::centred);

    g.setColour (juce::Colour (0xff909098));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String (smoothedFreq, 1) + " Hz  "
                + (cents >= 0.0f ? "+" : "") + juce::String (cents, 0) + " cent",
                bounds.removeFromTop (16), juce::Justification::centred);

    auto barArea = bounds.reduced (16, 4).removeFromTop (14);
    g.setColour (juce::Colour (0xff2a2a30));
    g.fillRoundedRectangle (barArea.toFloat(), 4.0f);

    const float norm = juce::jlimit (-1.0f, 1.0f, cents / 50.0f);
    const float cx = (float) barArea.getCentreX();
    const float half = (float) barArea.getWidth() * 0.5f;

    g.setColour (inTune ? juce::Colour (0xff40e080) : juce::Colour (0xff00c8ff));
    if (norm >= 0.0f)
        g.fillRoundedRectangle (cx, (float) barArea.getY(),
                                half * norm, (float) barArea.getHeight(), 3.0f);
    else
        g.fillRoundedRectangle (cx + half * norm, (float) barArea.getY(),
                                -half * norm, (float) barArea.getHeight(), 3.0f);

    g.setColour (juce::Colour (0xff707078));
    g.fillRect ((int) cx - 1, barArea.getY() - 2, 2, barArea.getHeight() + 4);
}

//==============================================================================
// BandsPage
//==============================================================================
BandsPage::BandsPage()
{
    styleKnob (*this, compS,      compL,      "COMP");
    styleKnob (*this, lowSatS,    lowSatL,    "SAT");
    styleKnob (*this, xoverS,     xoverL,     "XOVER");
    styleKnob (*this, lowLevelS,  lowLevelL,  "LEVEL");
    styleKnob (*this, driveS,     driveL,     "DRIVE");
    styleKnob (*this, toneS,      toneL,      "TONE");
    styleKnob (*this, highLevelS, highLevelL, "LEVEL");

    characterBox.addItemList ({ "Tube", "Rodent", "Fuzz" }, 1);
    addAndMakeVisible (characterBox);
    characterL.setText ("CHARACTER", juce::dontSendNotification);
    characterL.setJustificationType (juce::Justification::centred);
    characterL.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    characterL.setColour (juce::Label::textColourId, juce::Colour (0xff909098));
    addAndMakeVisible (characterL);
}

void BandsPage::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff707078));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("LOW BAND",  16, 8,  200, 16, juce::Justification::left);
    g.drawText ("HIGH BAND", 16, 116, 200, 16, juce::Justification::left);

    g.setColour (juce::Colour (0xff34343c));
    g.fillRect (12, 110, getWidth() - 24, 1);
}

void BandsPage::resized()
{
    const int knobW = 76, knobH = 74, labelH = 16;

    auto place = [&] (juce::Slider& s, juce::Label& l, int x, int y)
    {
        l.setBounds (x, y, knobW, labelH);
        s.setBounds (x, y + labelH, knobW, knobH);
    };

    // riga LOW
    place (compS,     compL,      30, 26);
    place (lowSatS,   lowSatL,   190, 26);
    place (xoverS,    xoverL,    350, 26);
    place (lowLevelS, lowLevelL, 510, 26);

    // riga HIGH
    place (driveS,     driveL,     30, 134);
    place (toneS,      toneL,     190, 134);
    place (highLevelS, highLevelL, 510, 134);

    characterL.setBounds   (350, 150, 100, labelH);
    characterBox.setBounds (350, 170, 100, 26);
}

//==============================================================================
// CabPage
//==============================================================================
CabPage::CabPage()
{
    cabBox.addItemList ({ "Off", "High Band", "Full Mix" }, 1);
    addAndMakeVisible (cabBox);

    cabL.setText ("POSIZIONE IR", juce::dontSendNotification);
    cabL.setJustificationType (juce::Justification::centredLeft);
    cabL.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    cabL.setColour (juce::Label::textColourId, juce::Colour (0xff909098));
    addAndMakeVisible (cabL);

    addAndMakeVisible (loadIRButton);

    irNameLabel.setJustificationType (juce::Justification::centredLeft);
    irNameLabel.setFont (juce::FontOptions (13.0f));
    irNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff909098));
    addAndMakeVisible (irNameLabel);
}

void CabPage::resized()
{
    cabL.setBounds         (30,  30, 150, 16);
    cabBox.setBounds       (30,  52, 150, 28);
    loadIRButton.setBounds (210, 52, 110, 28);
    irNameLabel.setBounds  (30,  96, getWidth() - 60, 22);
}

//==============================================================================
// ParallaxStyleEditor
//==============================================================================
ParallaxStyleEditor::ParallaxStyleEditor (ParallaxStyleProcessor& p)
    : AudioProcessorEditor (p), processor (p), tuner (p)
{
    setLookAndFeel (&lnf);

    // --- top bar ---
    styleKnob (*this, inputS,  inputL,  "INPUT");
    styleKnob (*this, gateS,   gateL,   "GATE");
    styleKnob (*this, blendS,  blendL,  "BLEND");
    styleKnob (*this, outputS, outputL, "OUTPUT");

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    inMeterL.setText ("IN", juce::dontSendNotification);
    outMeterL.setText ("OUT", juce::dontSendNotification);
    for (auto* l : { &inMeterL, &outMeterL })
    {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, juce::Colour (0xff707078));
        addAndMakeVisible (*l);
    }

    // --- tab centrale ---
    tabs.addTab ("BANDS",  juce::Colours::transparentBlack, &bandsPage, false);
    tabs.addTab ("CAB IR", juce::Colours::transparentBlack, &cabPage,   false);
    tabs.setTabBarDepth (30);
    addAndMakeVisible (tabs);

    cabPage.loadIRButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Seleziona un file IR", juce::File(), "*.wav;*.aif;*.aiff;*.flac");

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile())
                    processor.loadIR (file);
            });
    };

    // --- tuner ---
    addAndMakeVisible (tunerToggle);
    addAndMakeVisible (tuner);

    // --- attachments ---
    auto& vts = processor.apvts;
    auto attach = [&] (juce::Slider& s, const char* id)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (vts, id, s));
    };

    attach (inputS,               "input");
    attach (gateS,                "gate");
    attach (blendS,               "blend");
    attach (outputS,              "output");
    attach (bandsPage.compS,      "comp");
    attach (bandsPage.lowSatS,    "lowsat");
    attach (bandsPage.xoverS,     "xover");
    attach (bandsPage.lowLevelS,  "lowlevel");
    attach (bandsPage.driveS,     "drive");
    attach (bandsPage.toneS,      "tone");
    attach (bandsPage.highLevelS, "highlevel");

    characterAttachment = std::make_unique<ComboAttachment> (vts, "character", bandsPage.characterBox);
    cabAttachment       = std::make_unique<ComboAttachment> (vts, "cab", cabPage.cabBox);
    tunerAttachment     = std::make_unique<ButtonAttachment> (vts, "tuneron", tunerToggle);

    startTimerHz (30);
    setSize (720, 560);
}

ParallaxStyleEditor::~ParallaxStyleEditor()
{
    setLookAndFeel (nullptr);
}

void ParallaxStyleEditor::timerCallback()
{
    inMeter.pushLevel  (processor.consumeInputPeak());
    outMeter.pushLevel (processor.consumeOutputPeak());

    const auto irFile = processor.getIRFile();
    cabPage.irNameLabel.setText (irFile == juce::File() ? "nessuna IR caricata"
                                                        : irFile.getFileName(),
                                 juce::dontSendNotification);
}

//==============================================================================
void ParallaxStyleEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff202026), 0.0f, 0.0f,
                                             juce::Colour (0xff121216), 0.0f, bounds.getHeight(),
                                             false));
    g.fillAll();

    g.setColour (juce::Colour (0xff00c8ff));
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("PARALLAX STYLE", 20, 10, 300, 24, juce::Justification::left);

    auto drawPanel = [&g] (juce::Rectangle<int> r)
    {
        g.setColour (juce::Colour (0x30000000));
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff34343c));
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
    };

    drawPanel ({ 20,  40, 680, 128 });   // top bar
    drawPanel ({ 20, 428, 680, 116 });   // tuner
}

void ParallaxStyleEditor::resized()
{
    const int knobW = 76, knobH = 74, labelH = 16;

    auto place = [&] (juce::Slider& s, juce::Label& l, int x, int y)
    {
        l.setBounds (x, y, knobW, labelH);
        s.setBounds (x, y + labelH, knobW, knobH);
    };

    // --- top bar: INPUT GATE + meter IN | BLEND | meter OUT + OUTPUT ---
    place (inputS,  inputL,   30, 52);
    place (gateS,   gateL,   106, 52);
    place (blendS,  blendL,  322, 52);
    place (outputS, outputL, 594, 52);

    inMeterL.setBounds  (192,  74,  30, 14);
    inMeter.setBounds   (192,  92, 120, 16);
    outMeterL.setBounds (408,  74,  36, 14);
    outMeter.setBounds  (408,  92, 120, 16);

    // --- tab centrale ---
    tabs.setBounds (20, 178, 680, 240);

    // --- tuner ---
    tunerToggle.setBounds (36, 470, 90, 24);
    tuner.setBounds (200, 440, 320, 96);
}
