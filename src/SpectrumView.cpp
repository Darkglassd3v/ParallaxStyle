#include "SpectrumView.h"
#include "Theme.h"

#include <complex>

namespace
{
    constexpr float topDb    = 6.0f;     // scala verticale dello spettro
    constexpr float bottomDb = -78.0f;
    constexpr float eqRangeDb = 18.0f;   // la curva dell'EQ: ±18 dB attorno al centro
    constexpr float grabRadius = 14.0f;

    juce::RangedAudioParameter* findParam (juce::AudioProcessor& p, const juce::String& id)
    {
        for (auto* param : p.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                if (ranged->getParameterID() == id)
                    return ranged;
        jassertfalse;
        return nullptr;
    }

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

SpectrumView::SpectrumView (ParallaxStyleProcessor& p)
    : processor (p),
      inputSpectrum (p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0),
      outputSpectrum (p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0)
{
    handles = {
        { "lowfreq",  "LOW",  0.30f, theme::low,  findParam (p, "lowfreq")  },
        { "midfrom",  "MID",  0.52f, theme::mid,  findParam (p, "midfrom")  },
        { "midto",    "MID",  0.52f, theme::mid,  findParam (p, "midto")    },
        { "highfreq", "HIGH", 0.74f, theme::high, findParam (p, "highfreq") },
    };

    setTitle ("Spectrum and band crossovers");
    setDescription ("Drag a handle to move a band edge; double-click resets it.");
    processor.setAnalyzerActive (true);
    startTimerHz (30);
}

SpectrumView::~SpectrumView()
{
    stopTimer();
    processor.setAnalyzerActive (false);
}

float SpectrumView::valueOf (const juce::RangedAudioParameter& param) const
{
    return param.convertFrom0to1 (param.getValue());
}

juce::Point<float> SpectrumView::handlePosition (const juce::String& paramId) const
{
    const FrequencyAxis axis { (float) getWidth() };
    for (const auto& h : handles)
        if (paramId == h.paramId)
            return { axis.xForFrequency (valueOf (*h.param)), (float) getHeight() * h.heightFraction };
    jassertfalse;
    return {};
}

int SpectrumView::handleAt (juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = grabRadius;
    for (size_t i = 0; i < handles.size(); ++i)
    {
        const float d = position.getDistanceFrom (handlePosition (handles[i].paramId));
        if (d < bestDistance)
        {
            best = (int) i;
            bestDistance = d;
        }
    }
    return best;
}

bool SpectrumView::beginDragAt (juce::Point<float> position)
{
    dragging = handleAt (position);
    if (dragging < 0)
        return false;
    handles[(size_t) dragging].param->beginChangeGesture();
    return true;
}

void SpectrumView::dragTo (float x)
{
    if (dragging < 0)
        return;
    auto& handle = handles[(size_t) dragging];
    const FrequencyAxis axis { (float) getWidth() };
    float hz = axis.frequencyForX (x);

    // la banda MID non si rovescia: TO resta sopra FROM e FROM sotto TO
    if (juce::String (handle.paramId) == "midto")
        hz = juce::jmax (hz, valueOf (*handles[1].param) + 1.0f);
    else if (juce::String (handle.paramId) == "midfrom")
        hz = juce::jmin (hz, valueOf (*handles[2].param) - 1.0f);

    handle.param->setValueNotifyingHost (handle.param->convertTo0to1 (hz));   // fuori range: si ferma al limite
    repaint();
}

void SpectrumView::endDrag()
{
    if (dragging >= 0)
        handles[(size_t) dragging].param->endChangeGesture();
    dragging = -1;
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)  { beginDragAt (e.position); }
void SpectrumView::mouseDrag (const juce::MouseEvent& e)  { dragTo (e.position.x); }
void SpectrumView::mouseUp (const juce::MouseEvent&)      { endDrag(); }

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h < 0)
        return;
    auto& param = *handles[(size_t) h].param;
    param.beginChangeGesture();
    param.setValueNotifyingHost (param.getDefaultValue());
    param.endChangeGesture();
}

//==============================================================================
std::vector<float> SpectrumView::readShownParameters() const
{
    std::vector<float> values;
    for (auto* id : { "lowfreq", "midfrom", "midto", "highfreq", "lowon", "midon", "highon",
                      "eqlofreq", "eqlogain", "eqm1freq", "eqm1gain", "eqm2freq", "eqm2gain",
                      "eqhifreq", "eqhigain" })
        values.push_back (processor.apvts.getRawParameterValue (id)->load());
    return values;
}

bool SpectrumView::refresh()
{
    const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    inputSpectrum.setSampleRate (rate);
    outputSpectrum.setSampleRate (rate);

    const int gotIn = processor.readAnalyzerInput (scratch.data(), (int) scratch.size());
    inputSpectrum.push (scratch.data(), gotIn);
    const int gotOut = processor.readAnalyzerOutput (scratch.data(), (int) scratch.size());
    outputSpectrum.push (scratch.data(), gotOut);

    // senza segnale gli spettri continuano a scendere fino al fondo, poi si fermano
    std::fill_n (scratch.begin(), 512, 0.0f);
    if (gotIn == 0 && ! inputSpectrum.isSilent())   inputSpectrum.push (scratch.data(), 512);
    if (gotOut == 0 && ! outputSpectrum.isSilent()) outputSpectrum.push (scratch.data(), 512);

    const bool inputChanged  = inputSpectrum.update();
    const bool outputChanged = outputSpectrum.update();
    auto shown = readShownParameters();
    const bool parametersChanged = shown != shownParameters;
    shownParameters = std::move (shown);

    if (! (inputChanged || outputChanged || parametersChanged))
        return false;

    repaint();
    return true;
}

float SpectrumView::yForDb (float db) const noexcept
{
    return juce::jmap (db, topDb, bottomDb, 0.0f, (float) getHeight());
}

juce::Path SpectrumView::spectrumPath (const SpectrumAnalyzer& spectrum, bool closed) const
{
    const FrequencyAxis axis { (float) getWidth() };
    juce::Path p;
    const float bottom = (float) getHeight();
    for (float x = 0.0f; x <= (float) getWidth(); x += 2.0f)
    {
        const float y = juce::jmin (bottom, yForDb (spectrum.levelAt (axis.frequencyForX (x))));
        if (x == 0.0f)
        {
            if (closed) { p.startNewSubPath (0.0f, bottom); p.lineTo (0.0f, y); }
            else        p.startNewSubPath (0.0f, y);
        }
        else
        {
            p.lineTo (x, y);
        }
    }
    if (closed)
    {
        p.lineTo ((float) getWidth(), bottom);
        p.closeSubPath();
    }
    return p;
}

juce::Path SpectrumView::eqCurve() const
{
    using Coeffs = juce::dsp::IIR::ArrayCoefficients<float>;
    const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    auto& vts = processor.apvts;
    auto value = [&vts] (const char* id) { return vts.getRawParameterValue (id)->load(); };
    auto gain  = [&value] (const char* id) { return juce::Decibels::decibelsToGain (value (id)); };

    const std::array<std::array<float, 6>, 4> sections {
        Coeffs::makeLowShelf   (rate, value ("eqlofreq"), 0.8f, gain ("eqlogain")),
        Coeffs::makePeakFilter (rate, value ("eqm1freq"), 0.8f, gain ("eqm1gain")),
        Coeffs::makePeakFilter (rate, value ("eqm2freq"), 0.8f, gain ("eqm2gain")),
        Coeffs::makeHighShelf  (rate, value ("eqhifreq"), 0.8f, gain ("eqhigain")),
    };

    const FrequencyAxis axis { (float) getWidth() };
    const float centre = (float) getHeight() * 0.5f;
    juce::Path p;
    for (float x = 0.0f; x <= (float) getWidth(); x += 3.0f)
    {
        const double w = juce::MathConstants<double>::twoPi * axis.frequencyForX (x) / rate;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        double magnitude = 1.0;
        for (const auto& c : sections)   // b0 b1 b2 a0 a1 a2
            magnitude *= std::abs ((double) c[0] + (double) c[1] * z1 + (double) c[2] * z2)
                       / std::abs ((double) c[3] + (double) c[4] * z1 + (double) c[5] * z2);
        const float db = juce::jlimit (-eqRangeDb, eqRangeDb, (float) juce::Decibels::gainToDecibels (magnitude));
        const float y  = centre - db / eqRangeDb * centre * 0.9f;
        if (x == 0.0f) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}

void SpectrumView::drawBand (juce::Graphics& g, float fromHz, float toHz, juce::Colour colour, bool on) const
{
    const FrequencyAxis axis { (float) getWidth() };
    const float x1 = axis.xForFrequency (fromHz), x2 = axis.xForFrequency (toHz);
    const auto area = juce::Rectangle<float> (x1, 0.0f, juce::jmax (0.0f, x2 - x1), (float) getHeight());

    g.setGradientFill (juce::ColourGradient (colour.withAlpha (on ? 0.16f : 0.05f), 0.0f, 0.0f,
                                             colour.withAlpha (on ? 0.03f : 0.01f), 0.0f, (float) getHeight(), false));
    g.fillRect (area);
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::surface);
    g.fillRoundedRectangle (bounds, 10.0f);

    drawGrid (g);
    drawBands (g);
    drawSpectra (g);
    drawHandles (g);

    g.setColour (theme::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
}

bool SpectrumView::isOn (const char* bandSwitch) const
{
    return processor.apvts.getRawParameterValue (bandSwitch)->load() > 0.5f;
}

void SpectrumView::drawGrid (juce::Graphics& g) const
{
    const auto bounds = getLocalBounds().toFloat();
    const FrequencyAxis axis { bounds.getWidth() };

    // griglia: frequenze e livelli
    g.setFont (theme::font (10.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = axis.xForFrequency (hz);
        g.setColour (theme::border.withAlpha (0.6f));
        g.fillRect (x, 0.0f, 1.0f, bounds.getHeight());
        g.setColour (theme::muted.withAlpha (0.7f));
        g.drawText (hz >= 1000.0f ? juce::String ((int) hz / 1000) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x + 3.0f, bounds.getBottom() - 15.0f, 30.0f, 12.0f),
                    juce::Justification::centredLeft);
    }
    for (float db = -12.0f; db > bottomDb; db -= 12.0f)
    {
        g.setColour (theme::border.withAlpha (0.35f));
        g.fillRect (0.0f, yForDb (db), bounds.getWidth(), 1.0f);
    }
}

void SpectrumView::drawBands (juce::Graphics& g) const
{
    const float lowFreq  = valueOf (*handles[0].param);
    const float midFrom  = valueOf (*handles[1].param);
    const float midTo    = juce::jmax (valueOf (*handles[2].param), midFrom + 1.0f);
    const float highFreq = valueOf (*handles[3].param);
    drawBand (g, FrequencyAxis::minHz, lowFreq, theme::low, isOn ("lowon"));
    drawBand (g, midFrom, midTo, theme::mid, isOn ("midon"));
    drawBand (g, highFreq, FrequencyAxis::maxHz, theme::high, isOn ("highon"));
}

void SpectrumView::drawSpectra (juce::Graphics& g) const
{
    auto& vts = processor.apvts;

    g.setColour (theme::text.withAlpha (0.09f));
    g.fillPath (spectrumPath (inputSpectrum, true));
    g.setColour (theme::text.withAlpha (0.28f));
    g.strokePath (spectrumPath (inputSpectrum, false), juce::PathStrokeType (1.0f));
    g.setColour (theme::text.withAlpha (0.85f));
    g.strokePath (spectrumPath (outputSpectrum, false), juce::PathStrokeType (1.5f));

    // curva dell'EQ post, solo se l'EQ fa qualcosa
    bool eqActive = false;
    for (auto* id : { "eqlogain", "eqm1gain", "eqm2gain", "eqhigain" })
        eqActive = eqActive || std::abs (vts.getRawParameterValue (id)->load()) > 0.05f;
    if (eqActive)
    {
        g.setColour (theme::accent.withAlpha (0.9f));
        g.strokePath (eqCurve(), juce::PathStrokeType (1.5f));
    }
}

void SpectrumView::drawHandles (juce::Graphics& g) const
{
    const auto bounds = getLocalBounds().toFloat();

    // bordi delle bande e maniglie
    const bool bandOn[] = { isOn ("lowon"), isOn ("midon"), isOn ("midon"), isOn ("highon") };
    for (size_t i = 0; i < handles.size(); ++i)
    {
        const auto& h = handles[i];
        const auto  pos = handlePosition (h.paramId);
        const bool  active = (int) i == hovered || (int) i == dragging;
        const auto  colour = h.colour.withAlpha (bandOn[i] ? 1.0f : 0.4f);

        g.setColour (colour.withAlpha (bandOn[i] ? 0.75f : 0.3f));
        g.fillRect (pos.x - 0.75f, 0.0f, 1.5f, bounds.getHeight());

        const float r = active ? 8.0f : 6.0f;
        g.setColour (theme::background);
        g.fillEllipse (pos.x - r - 2.0f, pos.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f);
        g.setColour (colour);
        g.fillEllipse (pos.x - r, pos.y - r, r * 2.0f, r * 2.0f);

        // etichetta: nome della banda e frequenza, sul lato interno della banda
        const juce::String text = juce::String (h.band) + "  " + hzText (valueOf (*h.param));
        g.setFont (theme::caption());
        const float textW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f;
        // LOW e HIGH dentro la loro banda, MID fuori: le due etichette MID non si coprono
        const bool leftSide = juce::String (h.paramId) == "lowfreq" || juce::String (h.paramId) == "midfrom";
        auto pill = juce::Rectangle<float> (leftSide ? pos.x - r - 6.0f - textW : pos.x + r + 6.0f,
                                            pos.y - 9.0f, textW, 18.0f);
        pill = pill.withX (juce::jlimit (2.0f, bounds.getWidth() - textW - 2.0f, pill.getX()));
        g.setColour (theme::background.withAlpha (0.85f));
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (active ? theme::text : colour);
        g.drawText (text, pill, juce::Justification::centred);
    }
}
