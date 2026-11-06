#include "PluginEditor.h"

//==============================================================================
// Knob
//==============================================================================
void Knob::setup (juce::Component& parent, const juce::String& name, juce::Colour colour,
                  const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, width, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::textBoxTextColourId, theme::muted);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setTitle (name);
    parent.addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (theme::caption());
    label.setColour (juce::Label::textColourId, theme::muted);
    label.setBorderSize ({});
    label.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (label);
}

void Knob::setTopLeft (int x, int y)
{
    label.setBounds (x, y, width, 14);
    slider.setBounds (x, y + 14, width, height - 14);
}

//==============================================================================
// Card
//==============================================================================
Card::Card (const juce::String& t, const juce::String& s, juce::Colour c, bool hasPowerSwitch)
    : title (t), subtitle (s), colour (c)
{
    if (hasPowerSwitch)
    {
        power.setColour (juce::ToggleButton::tickColourId, colour);
        power.setTitle (title + " band on/off");
        power.onStateChange = [this] { repaint(); };
        addAndMakeVisible (power);
        isActive = [this] { return power.getToggleState(); };
    }
    dimmedArea = [this] { return getLocalBounds().withTrimmedTop (headerHeight); };
}

void Card::resized()
{
    power.setBounds (getWidth() - 48, 8, 40, 20);
}

void Card::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (theme::surface);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    const bool active = isActive == nullptr || isActive();
    g.setColour (active ? colour : colour.withAlpha (0.35f));
    g.fillEllipse (14.0f, 14.0f, 8.0f, 8.0f);

    g.setFont (theme::font (13.0f, true).withKerningFactor (0.08f));
    g.setColour (active ? theme::text : theme::muted);
    const float titleW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title);
    g.drawText (title, juce::Rectangle<float> (30.0f, 8.0f, titleW + 4.0f, 20.0f), juce::Justification::centredLeft);

    if (subtitle.isNotEmpty())
    {
        g.setFont (theme::caption());
        g.setColour (theme::muted);
        g.drawText (subtitle, juce::Rectangle<float> (40.0f + titleW, 8.0f, 160.0f, 20.0f), juce::Justification::centredLeft);
    }
}

void Card::paintOverChildren (juce::Graphics& g)
{
    if (isActive != nullptr && ! isActive())
    {
        g.setColour (theme::background.withAlpha (0.6f));
        g.fillRoundedRectangle (dimmedArea().toFloat(), 10.0f);
    }
}

//==============================================================================
// Pagine
//==============================================================================
namespace
{
    void setupChoice (juce::Component& parent, juce::ComboBox& box, juce::Label& label,
                      const juce::StringArray& items, const juce::String& name)
    {
        box.addItemList (items, 1);
        box.setTitle (name);
        parent.addAndMakeVisible (box);

        label.setText (name, juce::dontSendNotification);
        label.setFont (theme::caption());
        label.setColour (juce::Label::textColourId, theme::muted);
        label.setBorderSize ({});
        parent.addAndMakeVisible (label);
    }

    const juce::StringArray characters { "Tube", "Rodent", "Fuzz", "Doom", "X" };
}

BandsPage::BandsPage()
{
    for (auto* c : { &lowCard, &midCard, &highCard })
        addAndMakeVisible (*c);

    lowFreq.setup  (lowCard, "FREQ",  theme::low, " Hz");
    comp.setup     (lowCard, "COMP",  theme::low);
    lowSat.setup   (lowCard, "SAT",   theme::low);
    lowLevel.setup (lowCard, "LEVEL", theme::low, " dB");

    midFrom.setup  (midCard, "FROM",  theme::mid, " Hz");
    midTo.setup    (midCard, "TO",    theme::mid, " Hz");
    midGain.setup  (midCard, "GAIN",  theme::mid, " dB");
    midDrive.setup (midCard, "DRIVE", theme::mid, " dB");
    setupChoice (midCard, midCharacter, midCharacterLabel, characters, "CHARACTER");

    highFreq.setup  (highCard, "FROM",  theme::high, " Hz");
    drive.setup     (highCard, "DRIVE", theme::high, " dB");
    tone.setup      (highCard, "TONE",  theme::high, " Hz");
    highLevel.setup (highCard, "LEVEL", theme::high, " dB");
    setupChoice (highCard, character, characterLabel, characters, "CHARACTER");
}

void BandsPage::resized()
{
    constexpr int cardW = 280, gap = 14;
    lowCard.setBounds  (0,                  0, cardW, getHeight());
    midCard.setBounds  (cardW + gap,        0, cardW, getHeight());
    highCard.setBounds ((cardW + gap) * 2,  0, cardW, getHeight());

    auto row = [] (std::initializer_list<Knob*> knobs)
    {
        int x = 8;
        for (auto* k : knobs)
        {
            k->setTopLeft (x, Card::headerHeight + 8);
            x += Knob::width + 4;
        }
    };
    row ({ &lowFreq, &comp, &lowSat, &lowLevel });
    row ({ &midFrom, &midTo, &midGain, &midDrive });
    row ({ &highFreq, &drive, &tone, &highLevel });

    for (auto [label, box] : { std::pair { &midCharacterLabel, &midCharacter },
                               std::pair { &characterLabel, &character } })
    {
        label->setBounds (16, 150, 90, 26);
        box->setBounds (112, 150, 152, 26);
    }
}

EqPage::EqPage()
{
    for (auto* c : { &loCard, &m1Card, &m2Card, &hiCard })
        addAndMakeVisible (*c);

    loFreq.setup (loCard, "FREQ", theme::accent, " Hz");
    loGain.setup (loCard, "GAIN", theme::accent, " dB");
    m1Freq.setup (m1Card, "FREQ", theme::accent, " Hz");
    m1Gain.setup (m1Card, "GAIN", theme::accent, " dB");
    m2Freq.setup (m2Card, "FREQ", theme::accent, " Hz");
    m2Gain.setup (m2Card, "GAIN", theme::accent, " dB");
    hiFreq.setup (hiCard, "FREQ", theme::accent, " Hz");
    hiGain.setup (hiCard, "GAIN", theme::accent, " dB");
}

void EqPage::resized()
{
    constexpr int cardW = 205, gap = 16;
    const std::pair<Card*, std::pair<Knob*, Knob*>> cards[] = {
        { &loCard, { &loFreq, &loGain } }, { &m1Card, { &m1Freq, &m1Gain } },
        { &m2Card, { &m2Freq, &m2Gain } }, { &hiCard, { &hiFreq, &hiGain } },
    };

    int x = 0;
    for (auto& [card, knobs] : cards)
    {
        card->setBounds (x, 0, cardW, getHeight());
        knobs.first->setTopLeft (30, Card::headerHeight + 30);
        knobs.second->setTopLeft (cardW - 30 - Knob::width, Card::headerHeight + 30);
        x += cardW + gap;
    }
}

CabPage::CabPage()
{
    addAndMakeVisible (card);
    setupChoice (card, position, positionLabel, { "Off", "High Band", "Full Mix" }, "POSITION");
    card.addAndMakeVisible (loadIR);

    irName.setFont (theme::font (13.0f));
    irName.setColour (juce::Label::textColourId, theme::text);
    irName.setBorderSize ({});
    card.addAndMakeVisible (irName);

    irLevel.setup (card, "IR LEVEL", theme::accent, " dB");

    // spenta (Off) si oscura tutto tranne il selettore della posizione
    card.isActive   = [this] { return position.getSelectedItemIndex() > 0; };
    card.dimmedArea = [this] { return card.getLocalBounds().withTrimmedTop (96); };
    position.onChange = [this] { card.repaint(); };
}

void CabPage::resized()
{
    card.setBounds (getLocalBounds());
    positionLabel.setBounds (20, 54, 74, 28);
    position.setBounds (100, 54, 160, 28);
    loadIR.setBounds (276, 54, 112, 28);
    irName.setBounds (20, 128, 560, 24);
    irLevel.setTopLeft (card.getWidth() - Knob::width - 40, 112);
}

//==============================================================================
// TunerComponent
//==============================================================================
TunerComponent::TunerComponent (ParallaxStyleProcessor& p) : processor (p)
{
    pTunerOn = processor.apvts.getRawParameterValue ("tuneron");
    startTimerHz (20);
}

void TunerComponent::setShown (const juce::String& description)
{
    if (description != getDescription())
    {
        setDescription (description);   // gli screen reader leggono la nota
        repaint();
    }
}

void TunerComponent::update()
{
    if (pTunerOn->load() < 0.5f)
    {
        hasSignal = false;
        windowFill = 0;
        smoothedFreq = 0.0f;
        setShown ("Tuner off");
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
        setShown ({});
        return;
    }

    const float freq = pitch::detectYin (window.data(), windowSize, processor.getTunerSampleRate());
    if (freq > 0.0f)
    {
        smoothedFreq = smoothedFreq <= 0.0f ? freq
                     : 0.7f * smoothedFreq + 0.3f * freq;
        note = pitch::noteFor (smoothedFreq);
        hasSignal = true;
        setShown (note.name + ", " + pitch::readout (smoothedFreq));
    }
}

void TunerComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (pTunerOn->load() < 0.5f)
    {
        g.setColour (theme::muted.withAlpha (0.6f));
        g.setFont (theme::caption());
        g.drawText ("TUNER OFF", bounds, juce::Justification::centred);
        return;
    }

    if (! hasSignal)
    {
        g.setColour (theme::muted);
        g.setFont (theme::font (26.0f, true));
        g.drawText ("-", bounds.removeFromTop (36.0f), juce::Justification::centred);
        return;
    }

    const bool inTune = std::abs (note.cents) < 5.0f;

    g.setColour (inTune ? theme::low : theme::text);
    g.setFont (theme::font (26.0f, true));
    g.drawText (note.name, bounds.removeFromTop (34.0f), juce::Justification::centred);

    g.setColour (theme::muted);
    g.setFont (theme::font (11.0f));
    g.drawText (pitch::readout (smoothedFreq), bounds.removeFromTop (16.0f), juce::Justification::centred);

    auto bar = bounds.reduced (12.0f, 6.0f).withHeight (6.0f);
    g.setColour (theme::raised);
    g.fillRoundedRectangle (bar, 3.0f);

    const float norm = juce::jlimit (-1.0f, 1.0f, note.cents / 50.0f);
    const float cx = bar.getCentreX();
    const float half = bar.getWidth() * 0.5f;
    g.setColour (inTune ? theme::low : theme::accent);
    g.fillRoundedRectangle (norm >= 0.0f ? bar.withX (cx).withWidth (half * norm)
                                         : bar.withX (cx + half * norm).withWidth (-half * norm), 3.0f);
    g.setColour (theme::text);
    g.fillRect (cx - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f);
}

//==============================================================================
// ParallaxStyleEditor
//==============================================================================
ParallaxStyleEditor::ParallaxStyleEditor (ParallaxStyleProcessor& p)
    : AudioProcessorEditor (p), processor (p), spectrum (p), tuner (p)
{
    setLookAndFeel (&lnf);

    setupHeader();
    addAndMakeVisible (spectrum);
    setupPages();
    setupFooter();
    attachParameters();

    startTimerHz (30);
    setSize (900, 600);
}

void ParallaxStyleEditor::setupHeader()
{
    // --- header ---
    titleLabel.setText ("PARALLAX STYLE", juce::dontSendNotification);
    titleLabel.setFont (theme::font (17.0f, true).withKerningFactor (0.14f));
    titleLabel.setColour (juce::Label::textColourId, theme::text);
    titleLabel.setBorderSize ({});
    addAndMakeVisible (titleLabel);

    versionLabel.setText ("v" JucePlugin_VersionString, juce::dontSendNotification);
    versionLabel.setFont (theme::font (11.0f));
    versionLabel.setColour (juce::Label::textColourId, theme::muted);
    versionLabel.setBorderSize ({});
    addAndMakeVisible (versionLabel);

    for (auto* b : { &presetSaveB, &presetLoadB, &resetB })
        addAndMakeVisible (*b);

    // --- preset save/load ---
    const auto presetDir = juce::File::getSpecialLocation (
                               juce::File::userDocumentsDirectory)
                               .getChildFile ("ParallaxStyle");

    presetSaveB.onClick = [this, presetDir]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Salva preset", presetDir.getChildFile ("preset.pllx"), "*.pllx");

        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                    processor.savePreset (file.withFileExtension ("pllx"));
            });
    };

    presetLoadB.onClick = [this, presetDir]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Carica preset", presetDir, "*.pllx;*.xml");

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile())
                    processor.loadPreset (file);
            });
    };

    // --- reset ---
    resetB.onClick = [this] { processor.resetToDefaults(); };
}

void ParallaxStyleEditor::setupPages()
{
    // --- pagine ---
    tabs.addTab ("BANDS", juce::Colours::transparentBlack, &bandsPage, false);
    tabs.addTab ("EQ",    juce::Colours::transparentBlack, &eqPage,    false);
    tabs.addTab ("CAB",   juce::Colours::transparentBlack, &cabPage,   false);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    tabs.setIndent (0);
    addAndMakeVisible (tabs);

    cabPage.loadIR.onClick = [this]
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
}

void ParallaxStyleEditor::setupFooter()
{
    // --- footer ---
    input.setup  (*this, "INPUT",  theme::accent, " dB");
    gate.setup   (*this, "GATE",   theme::accent, " dB");
    blend.setup  (*this, "BLEND",  theme::accent);
    output.setup (*this, "OUTPUT", theme::accent, " dB");

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    for (auto [l, text] : { std::pair { &inMeterLabel, "IN" }, std::pair { &outMeterLabel, "OUT" } })
    {
        l->setText (text, juce::dontSendNotification);
        l->setFont (theme::caption());
        l->setColour (juce::Label::textColourId, theme::muted);
        l->setBorderSize ({});
        addAndMakeVisible (*l);
    }

    tunerToggle.setColour (juce::ToggleButton::tickColourId, theme::accent);
    addAndMakeVisible (tunerToggle);
    addAndMakeVisible (tuner);
}

void ParallaxStyleEditor::attachParameters()
{
    // --- attachments ---
    auto& vts = processor.apvts;
    auto attach = [&] (Knob& k, const char* id)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (vts, id, k.slider));
        k.slider.setTitle (vts.getParameter (id)->getName (64));
    };
    auto& b = bandsPage;
    auto& e = eqPage;
    for (auto [knob, id] : std::initializer_list<std::pair<Knob*, const char*>> {
             { &input, "input" }, { &gate, "gate" }, { &blend, "blend" }, { &output, "output" },
             { &b.lowFreq, "lowfreq" }, { &b.comp, "comp" }, { &b.lowSat, "lowsat" }, { &b.lowLevel, "lowlevel" },
             { &b.midFrom, "midfrom" }, { &b.midTo, "midto" }, { &b.midGain, "midgain" }, { &b.midDrive, "middrive" },
             { &b.highFreq, "highfreq" }, { &b.drive, "drive" }, { &b.tone, "tone" }, { &b.highLevel, "highlevel" },
             { &cabPage.irLevel, "cablevel" },
             { &e.loFreq, "eqlofreq" }, { &e.loGain, "eqlogain" }, { &e.m1Freq, "eqm1freq" }, { &e.m1Gain, "eqm1gain" },
             { &e.m2Freq, "eqm2freq" }, { &e.m2Gain, "eqm2gain" }, { &e.hiFreq, "eqhifreq" }, { &e.hiGain, "eqhigain" } })
        attach (*knob, id);

    comboAttachments.push_back (std::make_unique<ComboAttachment> (vts, "character", b.character));
    comboAttachments.push_back (std::make_unique<ComboAttachment> (vts, "midchar",   b.midCharacter));
    comboAttachments.push_back (std::make_unique<ComboAttachment> (vts, "cab",       cabPage.position));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (vts, "tuneron", tunerToggle));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (vts, "lowon",   b.lowCard.power));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (vts, "midon",   b.midCard.power));
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (vts, "highon",  b.highCard.power));
}

ParallaxStyleEditor::~ParallaxStyleEditor()
{
    setLookAndFeel (nullptr);
}

void ParallaxStyleEditor::refreshIndicators()
{
    inMeter.pushLevel  (processor.consumeInputPeak());
    outMeter.pushLevel (processor.consumeOutputPeak());

    const auto irFile = processor.getIRFile();
    cabPage.irName.setText (irFile == juce::File() ? "Nessuna IR caricata"
                                                   : irFile.getFileName(),
                            juce::dontSendNotification);
}

//==============================================================================
void ParallaxStyleEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    // pannello del footer
    const auto footer = juce::Rectangle<float> (16.0f, 506.0f, 868.0f, 86.0f);
    g.setColour (theme::surface);
    g.fillRoundedRectangle (footer, 10.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (footer.reduced (0.5f), 10.0f, 1.0f);
}

void ParallaxStyleEditor::resized()
{
    // --- header ---
    const int titleW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (titleLabel.getFont(), titleLabel.getText())) + 2;
    titleLabel.setBounds (16, 12, titleW, 26);
    versionLabel.setBounds (titleLabel.getRight() + 10, 18, 60, 16);

    resetB.setBounds      (884 - 72, 12, 72, 26);
    presetLoadB.setBounds (resetB.getX() - 8 - 64, 12, 64, 26);
    presetSaveB.setBounds (presetLoadB.getX() - 8 - 64, 12, 64, 26);

    // --- spettro e pagine ---
    spectrum.setBounds (16, 48, 868, 192);
    tabs.setBounds (16, 248, 868, 250);

    // --- footer ---
    input.setTopLeft (24, 509);
    gate.setTopLeft  (96, 509);

    inMeterLabel.setBounds (172, 541, 26, 14);
    inMeter.setBounds      (200, 543, 116, 10);

    tunerToggle.setBounds (332, 536, 92, 24);
    tuner.setBounds       (428, 510, 168, 78);

    outMeterLabel.setBounds (608, 541, 30, 14);
    outMeter.setBounds      (640, 543, 100, 10);

    blend.setTopLeft  (748, 509);
    output.setTopLeft (816, 509);
}
