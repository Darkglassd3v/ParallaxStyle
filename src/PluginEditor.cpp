#include "PluginEditor.h"

//==============================================================================
// TunerComponent
//==============================================================================
TunerComponent::TunerComponent (ParallaxStyleProcessor& p) : processor (p)
{
    startTimerHz (20);
}

void TunerComponent::timerCallback()
{
    // Scarica tutto il FIFO in un buffer rolling
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

    // Gate su RMS: sotto soglia non mostrare nulla
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

        // MIDI note number (A4 = 440 Hz = 69)
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
    const int minTau = juce::jmax (2, (int) (sr / 400.0));   // <= 400 Hz: range basso

    std::vector<float> diff ((size_t) maxTau, 0.0f);

    // Difference function
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

    // Cumulative mean normalized difference
    std::vector<float> cmnd ((size_t) maxTau, 1.0f);
    float runningSum = 0.0f;
    for (int tau = minTau; tau < maxTau; ++tau)
    {
        runningSum += diff[(size_t) tau];
        cmnd[(size_t) tau] = runningSum > 0.0f
                           ? diff[(size_t) tau] * (float) (tau - minTau + 1) / runningSum
                           : 1.0f;
    }

    // Primo minimo sotto soglia
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

    // Interpolazione parabolica
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

    // Barra dei cents: -50 .. +50
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
// ParallaxStyleEditor
//==============================================================================
ParallaxStyleEditor::ParallaxStyleEditor (ParallaxStyleProcessor& p)
    : AudioProcessorEditor (p), processor (p), tuner (p)
{
    setLookAndFeel (&lnf);

    setupKnob (xoverS,     xoverL,     "XOVER");
    setupKnob (compS,      compL,      "COMP");
    setupKnob (lowSatS,    lowSatL,    "SAT");
    setupKnob (lowLevelS,  lowLevelL,  "LEVEL");
    setupKnob (driveS,     driveL,     "DRIVE");
    setupKnob (toneS,      toneL,      "TONE");
    setupKnob (highLevelS, highLevelL, "LEVEL");
    setupKnob (blendS,     blendL,     "BLEND");
    setupKnob (outputS,    outputL,    "OUTPUT");

    characterBox.addItemList ({ "Tube", "Rodent", "Fuzz" }, 1);
    addAndMakeVisible (characterBox);
    characterL.setText ("CHARACTER", juce::dontSendNotification);
    characterL.setJustificationType (juce::Justification::centred);
    characterL.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    characterL.setColour (juce::Label::textColourId, juce::Colour (0xff909098));
    addAndMakeVisible (characterL);

    cabBox.addItemList ({ "Off", "High Band", "Full Mix" }, 1);
    addAndMakeVisible (cabBox);
    cabL.setText ("CAB IR", juce::dontSendNotification);
    cabL.setJustificationType (juce::Justification::centredLeft);
    cabL.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    cabL.setColour (juce::Label::textColourId, juce::Colour (0xff909098));
    addAndMakeVisible (cabL);

    addAndMakeVisible (loadIRButton);
    loadIRButton.onClick = [this]
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

    irNameLabel.setJustificationType (juce::Justification::centredLeft);
    irNameLabel.setFont (juce::FontOptions (12.0f));
    irNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff909098));
    addAndMakeVisible (irNameLabel);

    addAndMakeVisible (tuner);

    auto& vts = processor.apvts;
    auto attach = [&] (juce::Slider& s, const char* id)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (vts, id, s));
    };

    attach (xoverS,     "xover");
    attach (compS,      "comp");
    attach (lowSatS,    "lowsat");
    attach (lowLevelS,  "lowlevel");
    attach (driveS,     "drive");
    attach (toneS,      "tone");
    attach (highLevelS, "highlevel");
    attach (blendS,     "blend");
    attach (outputS,    "output");

    characterAttachment = std::make_unique<ComboAttachment> (vts, "character", characterBox);
    cabAttachment       = std::make_unique<ComboAttachment> (vts, "cab", cabBox);

    startTimerHz (2);   // refresh label IR
    setSize (720, 480);
}

ParallaxStyleEditor::~ParallaxStyleEditor()
{
    setLookAndFeel (nullptr);
}

void ParallaxStyleEditor::timerCallback()
{
    const auto irFile = processor.getIRFile();
    irNameLabel.setText (irFile == juce::File() ? "nessuna IR caricata"
                                                : irFile.getFileName(),
                         juce::dontSendNotification);
}

void ParallaxStyleEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, juce::Colour (0xff909098));
    addAndMakeVisible (l);
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
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("PARALLAX STYLE", 20, 12, 300, 28, juce::Justification::left);
    g.setColour (juce::Colour (0xff606068));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("parallel bass processor", 20, 36, 300, 16, juce::Justification::left);

    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (juce::Colour (0x30000000));
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff34343c));
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.0f);
        g.setColour (juce::Colour (0xff707078));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, r.getX() + 12, r.getY() + 8, r.getWidth() - 24, 16,
                    juce::Justification::left);
    };

    drawPanel ({  20,  64, 250, 276 }, "LOW BAND");
    drawPanel ({ 280,  64, 250, 276 }, "HIGH BAND");
    drawPanel ({ 540,  64, 160, 276 }, "MASTER");
    drawPanel ({  20, 352, 340, 112 }, "CAB SIM");
    drawPanel ({ 372, 352, 328, 112 }, "TUNER");
}

void ParallaxStyleEditor::resized()
{
    const int knobW = 76, knobH = 96, labelH = 16;

    auto place = [&] (juce::Slider& s, juce::Label& l, int x, int y)
    {
        l.setBounds (x, y, knobW, labelH);
        s.setBounds (x, y + labelH, knobW, knobH);
    };

    // LOW BAND
    place (compS,     compL,      40,  96);
    place (lowSatS,   lowSatL,   162,  96);
    place (xoverS,    xoverL,     40, 220);
    place (lowLevelS, lowLevelL, 162, 220);

    // HIGH BAND
    place (driveS,     driveL,     300,  96);
    place (toneS,      toneL,      422,  96);
    place (highLevelS, highLevelL, 422, 220);

    characterL.setBounds   (300, 236, 100, labelH);
    characterBox.setBounds (300, 256, 100, 26);

    // MASTER
    place (blendS,  blendL,  582,  96);
    place (outputS, outputL, 582, 220);

    // CAB SIM
    cabL.setBounds         ( 36, 380,  80, labelH);
    cabBox.setBounds       ( 36, 400, 110, 26);
    loadIRButton.setBounds (160, 400,  90, 26);
    irNameLabel.setBounds  ( 36, 432, 308, 20);

    // TUNER
    tuner.setBounds (380, 376, 312, 84);
}
