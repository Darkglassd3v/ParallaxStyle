#include "PluginProcessor.h"
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
 #include "PluginEditor.h"
#endif

//==============================================================================
namespace
{
    inline float shapeTube (float x) noexcept   { return std::tanh (1.2f * x); }

    inline float shapeRodent (float x) noexcept
    {
        // Hard clip stretto stile RAT spinta: molto più cattivo
        return juce::jlimit (-0.65f, 0.65f, 2.0f * x) * (1.0f / 0.65f);
    }

    inline float shapeFuzz (float x) noexcept
    {
        const float s = x >= 0.0f ? 1.0f : -1.0f;
        const float k = x >= 0.0f ? 3.5f : 4.5f;
        return s * (1.0f - std::exp (-k * std::abs (x)));
    }

    inline float shapeDoom (float x) noexcept
    {
        // Boost feroce + bias asimmetrico + hard clip: quasi square wave
        return juce::jlimit (-1.0f, 1.0f, 3.0f * x - 0.15f);
    }

    // Stadio inverter CMOS (stile CD4049): tanh con bias, DC ricentrato
    inline float cmosStage (float x, float bias) noexcept
    {
        return std::tanh (2.2f * (x + bias)) - std::tanh (2.2f * bias);
    }

    inline float shapeX (float x) noexcept
    {
        // Due stadi invertenti in cascata, bias asimmetrici diversi:
        // il carattere Microtubes/CMOS con armoniche pari dal bias
        float y = -cmosStage (x, 0.12f);
        y = -cmosStage (0.9f * y, 0.08f);
        return y;
    }

    inline float softSat (float x) noexcept     { return x / (1.0f + std::abs (x)); }

    inline void applyShaper (float* d, int n, int character) noexcept
    {
        switch (character)
        {
            case 0: for (int i = 0; i < n; ++i) d[i] = shapeTube   (d[i]); break;
            case 1: for (int i = 0; i < n; ++i) d[i] = shapeRodent (d[i]); break;
            case 2: for (int i = 0; i < n; ++i) d[i] = shapeFuzz   (d[i]); break;
            case 3: for (int i = 0; i < n; ++i) d[i] = shapeDoom   (d[i]); break;
            case 4: for (int i = 0; i < n; ++i) d[i] = shapeX      (d[i]); break;
            default: break;
        }
    }
}

//==============================================================================
ParallaxStyleProcessor::ParallaxStyleProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pInput     = apvts.getRawParameterValue ("input");
    pGate      = apvts.getRawParameterValue ("gate");
    pLowFreq   = apvts.getRawParameterValue ("lowfreq");
    pMidFrom   = apvts.getRawParameterValue ("midfrom");
    pMidTo     = apvts.getRawParameterValue ("midto");
    pMidGain   = apvts.getRawParameterValue ("midgain");
    pMidDrive  = apvts.getRawParameterValue ("middrive");
    pMidChar   = apvts.getRawParameterValue ("midchar");
    pHighFreq  = apvts.getRawParameterValue ("highfreq");
    pComp      = apvts.getRawParameterValue ("comp");
    pLowSat    = apvts.getRawParameterValue ("lowsat");
    pLowLevel  = apvts.getRawParameterValue ("lowlevel");
    pDrive     = apvts.getRawParameterValue ("drive");
    pCharacter = apvts.getRawParameterValue ("character");
    pTone      = apvts.getRawParameterValue ("tone");
    pHighLevel = apvts.getRawParameterValue ("highlevel");
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    pCab       = apvts.getRawParameterValue ("cab");
    pCabLevel  = apvts.getRawParameterValue ("cablevel");
#endif
    pBlend     = apvts.getRawParameterValue ("blend");
    pOutput    = apvts.getRawParameterValue ("output");
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    pTunerOn   = apvts.getRawParameterValue ("tuneron");
#endif
    pLowOn     = apvts.getRawParameterValue ("lowon");
    pMidOn     = apvts.getRawParameterValue ("midon");
    pHighOn    = apvts.getRawParameterValue ("highon");
    pEqLoF     = apvts.getRawParameterValue ("eqlofreq");
    pEqLoG     = apvts.getRawParameterValue ("eqlogain");
    pEqM1F     = apvts.getRawParameterValue ("eqm1freq");
    pEqM1G     = apvts.getRawParameterValue ("eqm1gain");
    pEqM2F     = apvts.getRawParameterValue ("eqm2freq");
    pEqM2G     = apvts.getRawParameterValue ("eqm2gain");
    pEqHiF     = apvts.getRawParameterValue ("eqhifreq");
    pEqHiG     = apvts.getRawParameterValue ("eqhigain");
#if PARALLAXSTYLE_ANAGRAM_BUILD
    bypassParam = apvts.getParameter ("bypass");
    pBypass     = apvts.getRawParameterValue ("bypass");
#endif

    // banner con la versione: lo Standalone lo scrive in console all'avvio
    juce::Logger::writeToLog ("ParallaxStyle " JucePlugin_VersionString);
}

juce::AudioProcessorValueTreeState::ParameterLayout
ParallaxStyleProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P> (juce::ParameterID { "input", 1 },
        "Input", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    // -80 dB = gate di fatto aperto (default)
    params.push_back (std::make_unique<P> (juce::ParameterID { "gate", 1 },
        "Gate", juce::NormalisableRange<float> (-80.0f, -20.0f, 0.1f), -80.0f));

    // LOW: comprimi fino a...
    params.push_back (std::make_unique<P> (juce::ParameterID { "lowfreq", 1 },
        "Low Freq", juce::NormalisableRange<float> (60.0f, 500.0f, 1.0f, 0.5f), 200.0f));

    // MID: banda da boostare/attenuare
    params.push_back (std::make_unique<P> (juce::ParameterID { "midfrom", 1 },
        "Mid From", juce::NormalisableRange<float> (80.0f, 1000.0f, 1.0f, 0.5f), 200.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "midto", 1 },
        "Mid To", juce::NormalisableRange<float> (200.0f, 5000.0f, 1.0f, 0.5f), 600.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "midgain", 1 },
        "Mid Gain", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "middrive", 1 },
        "Mid Drive", juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "midchar", 1 }, "Mid Character",
        juce::StringArray { "Tube", "Rodent", "Fuzz", "Doom", "X" }, 0));

    // HIGH: distorci da... in su (100 Hz = fuzz lanoso, 1k+ = definizione chirurgica)
    params.push_back (std::make_unique<P> (juce::ParameterID { "highfreq", 1 },
        "High Freq", juce::NormalisableRange<float> (100.0f, 2000.0f, 1.0f, 0.5f), 400.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "comp", 1 },
        "Low Comp", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "lowsat", 1 },
        "Low Sat", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "lowlevel", 1 },
        "Low Level", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "drive", 1 },
        "Drive", juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 18.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "character", 1 }, "Character",
        juce::StringArray { "Tube", "Rodent", "Fuzz", "Doom", "X" }, 0));

    params.push_back (std::make_unique<P> (juce::ParameterID { "tone", 1 },
        "Tone", juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.4f), 5000.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "highlevel", 1 },
        "High Level", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "cab", 1 }, "Cab IR",
        juce::StringArray { "Off", "High Band", "Full Mix" }, 0));

    params.push_back (std::make_unique<P> (juce::ParameterID { "cablevel", 1 },
        "IR Level", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
#endif

    params.push_back (std::make_unique<P> (juce::ParameterID { "blend", 1 },
        "Blend", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "output", 1 },
        "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    // EQ post globale
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqlofreq", 1 },
        "EQ Lo Freq", juce::NormalisableRange<float> (40.0f, 250.0f, 1.0f, 0.5f), 80.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqlogain", 1 },
        "EQ Lo Gain", juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "eqm1freq", 1 },
        "EQ Mid1 Freq", juce::NormalisableRange<float> (100.0f, 2000.0f, 1.0f, 0.5f), 400.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqm1gain", 1 },
        "EQ Mid1 Gain", juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "eqm2freq", 1 },
        "EQ Mid2 Freq", juce::NormalisableRange<float> (500.0f, 8000.0f, 1.0f, 0.5f), 1500.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqm2gain", 1 },
        "EQ Mid2 Gain", juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "eqhifreq", 1 },
        "EQ Hi Freq", juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.5f), 4000.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "eqhigain", 1 },
        "EQ Hi Gain", juce::NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f));

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "tuneron", 1 }, "Tuner", true));
#endif

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "lowon", 1 }, "Low On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "midon", 1 }, "Mid On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "highon", 1 }, "High On", true));

#if PARALLAXSTYLE_ANAGRAM_BUILD
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));
#endif

    return { params.begin(), params.end() };
}

//==============================================================================
void ParallaxStyleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    currentSampleRate = sampleRate;
    maxBlockSize      = (int) spec.maximumBlockSize;

    inputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.02);

    // Hard gate: costanti di tempo fisse tarate per basso
    gateEnvCoeff    = std::exp (-1.0f / (float) (0.030 * sampleRate));  // envelope release 30 ms
    gateOpenCoeff   = std::exp (-1.0f / (float) (0.0005 * sampleRate)); // apertura 0.5 ms
    gateCloseCoeff  = std::exp (-1.0f / (float) (0.020 * sampleRate));  // chiusura 20 ms
    gateHoldSamples = (int) (0.040 * sampleRate);                       // hold 40 ms

    lowpass.prepare (spec);
    lowpass.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

    highpass.prepare (spec);
    highpass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

    midHighpass.prepare (spec);
    midHighpass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

    midLowpass.prepare (spec);
    midLowpass.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

    midGain.prepare (spec);
    midGain.setRampDurationSeconds (0.02);

    compressor.prepare (spec);
    compressor.setAttack (5.0f);
    compressor.setRelease (100.0f);

    lowGain.prepare (spec);
    lowGain.setRampDurationSeconds (0.02);

    driveGain.prepare (spec);
    driveGain.setRampDurationSeconds (0.02);

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);

    highGain.prepare (spec);
    highGain.setRampDurationSeconds (0.02);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    convolution.prepare (spec);

    cabGain.prepare (spec);
    cabGain.setRampDurationSeconds (0.02);
#endif

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);

    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);

    eqLo.prepare (spec);
    eqM1.prepare (spec);
    eqM2.prepare (spec);
    eqHi.prepare (spec);

    lowBuffer.setSize    ((int) spec.numChannels, maxBlockSize);
    midBuffer.setSize    ((int) spec.numChannels, maxBlockSize);
    midDryBuffer.setSize ((int) spec.numChannels, maxBlockSize);
    highBuffer.setSize   ((int) spec.numChannels, maxBlockSize);
#if PARALLAXSTYLE_ANAGRAM_BUILD
    bypassBuffer.setSize ((int) spec.numChannels, maxBlockSize);
#endif

    forEachSmoother ([sampleRate] (auto& s) { s.reset (sampleRate, smoothingSeconds); });
    updateTargets();
    reset();

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    tunerSampleRate = sampleRate / (double) tunerDecimation;
#endif
}

void ParallaxStyleProcessor::reset()
{
    // Tutto lo stato interno torna a zero e ogni rampa salta al suo valore finale:
    // due istanze resettate insieme (kx:Reset su Anagram, due mono affiancate)
    // da qui in poi danno la stessa uscita per lo stesso ingresso.
    inputGain.reset();
    lowpass.reset();
    highpass.reset();
    midHighpass.reset();
    midLowpass.reset();
    compressor.reset();
    lowGain.reset();
    midGain.reset();
    driveGain.reset();
    toneFilter.reset();
    highGain.reset();
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    convolution.reset();
    cabGain.reset();
#endif
    outputGain.reset();
    dryWet.reset();
    eqLo.reset();
    eqM1.reset();
    eqM2.reset();
    eqHi.reset();

    gateEnvelope  = 0.0f;
    gateGainState = 1.0f;
    gateHoldCount = 0;
    gateIsOpen    = true;

    forEachSmoother ([] (auto& s) { s.setCurrentAndTargetValue (s.getTargetValue()); });
    updateEqCoefficients();
}

bool ParallaxStyleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
void ParallaxStyleProcessor::pushTunerSamples (const juce::AudioBuffer<float>& input)
{
    const int numChannels = input.getNumChannels();
    const int numSamples  = input.getNumSamples();
    const float chScale   = 1.0f / (float) juce::jmax (1, numChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += input.getSample (ch, i);
        mono *= chScale;

        decimAccum += mono;
        if (++decimCount >= tunerDecimation)
        {
            const float value = decimAccum / (float) tunerDecimation;
            decimAccum = 0.0f;
            decimCount = 0;

            int start1, size1, start2, size2;
            tunerFifo.prepareToWrite (1, start1, size1, start2, size2);
            if (size1 > 0)
                tunerFifoBuffer[(size_t) start1] = value;
            tunerFifo.finishedWrite (size1 + size2);
        }
    }
}

int ParallaxStyleProcessor::readTunerSamples (float* dest, int maxSamples)
{
    int start1, size1, start2, size2;
    tunerFifo.prepareToRead (maxSamples, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i) dest[i]         = tunerFifoBuffer[(size_t)(start1 + i)];
    for (int i = 0; i < size2; ++i) dest[size1 + i] = tunerFifoBuffer[(size_t)(start2 + i)];

    tunerFifo.finishedRead (size1 + size2);
    return size1 + size2;
}
#endif

void ParallaxStyleProcessor::accumulatePeak (std::atomic<float>& target,
                                             const juce::AudioBuffer<float>& buf)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buf.getMagnitude (ch, 0, buf.getNumSamples()));

    float cur = target.load (std::memory_order_relaxed);
    while (peak > cur && ! target.compare_exchange_weak (cur, peak,
                                                         std::memory_order_relaxed))
        {}
}

//==============================================================================
void ParallaxStyleProcessor::updateTargets()
{
    lowFreqS.setTargetValue  (pLowFreq->load());
    midFromS.setTargetValue  (pMidFrom->load());
    midToS.setTargetValue    (pMidTo->load());
    highFreqS.setTargetValue (pHighFreq->load());
    toneS.setTargetValue     (pTone->load());
    compS.setTargetValue     (pComp->load());
    lowSatS.setTargetValue   (pLowSat->load());

    const float midDrive = pMidDrive->load();
    midDriveS.setTargetValue (midDrive);
    midMixS.setTargetValue   (midDrive > 0.01f ? 1.0f : 0.0f);   // sotto 0.01 dB shaper escluso

    std::atomic<float>* const eqFreqs[] = { pEqLoF, pEqM1F, pEqM2F, pEqHiF };
    std::atomic<float>* const eqGains[] = { pEqLoG, pEqM1G, pEqM2G, pEqHiG };
    for (size_t i = 0; i < 4; ++i)
    {
        eqFreqS[i].setTargetValue (eqFreqs[i]->load());
        eqGainS[i].setTargetValue (eqGains[i]->load());
    }

    lowOnS.setTargetValue  (pLowOn->load()  > 0.5f ? 1.0f : 0.0f);
    midOnS.setTargetValue  (pMidOn->load()  > 0.5f ? 1.0f : 0.0f);
    highOnS.setTargetValue (pHighOn->load() > 0.5f ? 1.0f : 0.0f);
#if PARALLAXSTYLE_ANAGRAM_BUILD
    bypassS.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);
#endif

    // i Gain hanno già la loro rampa interna, campione per campione
    inputGain.setGainDecibels  (pInput->load());
    lowGain.setGainDecibels    (pLowLevel->load());
    midGain.setGainDecibels    (pMidGain->load());
    driveGain.setGainDecibels  (pDrive->load());
    highGain.setGainDecibels   (pHighLevel->load());
    outputGain.setGainDecibels (pOutput->load());
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    cabGain.setGainDecibels    (pCabLevel->load());
#endif
    dryWet.setWetMixProportion (pBlend->load());
}

bool ParallaxStyleProcessor::anySmoothing() const noexcept
{
    bool smoothing = false;
    const_cast<ParallaxStyleProcessor*> (this)->forEachSmoother ([&smoothing] (auto& s)
    {
        smoothing = smoothing || s.isSmoothing();
    });
    return smoothing;
}

void ParallaxStyleProcessor::updateEqCoefficients()
{
    using Coeffs = juce::dsp::IIR::ArrayCoefficients<float>;
    constexpr float q = 0.8f;

    auto gain = [this] (size_t i) { return juce::Decibels::decibelsToGain (eqGainS[i].getCurrentValue()); };

    // ArrayCoefficients non allocano: si possono ricalcolare nel thread audio
    *eqLo.state = Coeffs::makeLowShelf   (currentSampleRate, eqFreqS[0].getCurrentValue(), q, gain (0));
    *eqM1.state = Coeffs::makePeakFilter (currentSampleRate, eqFreqS[1].getCurrentValue(), q, gain (1));
    *eqM2.state = Coeffs::makePeakFilter (currentSampleRate, eqFreqS[2].getCurrentValue(), q, gain (2));
    *eqHi.state = Coeffs::makeHighShelf  (currentSampleRate, eqFreqS[3].getCurrentValue(), q, gain (3));
}

//==============================================================================
namespace
{
    // Compressore da basso "spremuto": il knob 0..1 spinge threshold giù fino a
    // -50 dB, ratio da 2:1 fino a 12:1, e applica makeup gain automatica così la
    // riduzione si SENTE come densità, non come semplice attenuazione.
    float compThresholdDb (float comp) noexcept { return juce::jmap (comp, 0.0f, 1.0f, 0.0f, -50.0f); }
    float compRatio       (float comp) noexcept { return juce::jmap (comp, 0.0f, 1.0f, 2.0f, 12.0f); }

    // makeup ~ metà della riduzione max teorica al threshold
    float compMakeupDb (float comp) noexcept
    {
        return comp * (-compThresholdDb (comp)) * (1.0f - 1.0f / compRatio (comp)) * 0.6f;
    }

    // Applica a ogni canale un guadagno 0..1 che va da `from` a `to` nel blocco.
    void applyFade (juce::AudioBuffer<float>& b, int numSamples, float from, float to)
    {
        if (from < 1.0f || to < 1.0f)
            b.applyGainRamp (0, numSamples, from, to);
    }
}

void ParallaxStyleProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    // Su Anagram i registri flush-to-zero li imposta l'host: un plugin non deve toccarli.
    juce::ScopedNoDenormals noDenormals;
#endif

    updateTargets();

    // L'host può mandare blocchi più grandi di quelli annunciati in prepareToPlay:
    // si elaborano a pezzi. Mentre un parametro scorre verso il nuovo valore i pezzi
    // sono brevi, così filtri, compressore ed EQ seguono la rampa senza scatti.
    const int numSamples = buffer.getNumSamples();
    for (int start = 0; start < numSamples;)
    {
        const int length = juce::jmin (numSamples - start,
                                       anySmoothing() ? smoothingChunk : maxBlockSize);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(),
                                        buffer.getNumChannels(), start, length);
        processChunk (chunk);
        start += length;
    }
}

void ParallaxStyleProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

#if PARALLAXSTYLE_ANAGRAM_BUILD
    // Bypass morbido richiesto da Anagram (lv2:enabled): dissolvenza di 50 ms
    // tra il segnale elaborato e l'ingresso; a bypass completo nessuna elaborazione.
    const float bypassFrom = bypassS.getCurrentValue();
    const float bypassTo   = bypassS.skip (numSamples);
    if (bypassFrom >= 1.0f && bypassTo >= 1.0f)
    {
        accumulatePeak (inputPeak, buffer);
        accumulatePeak (outputPeak, buffer);
        return;
    }
    const bool bypassFading = bypassFrom > 0.0f || bypassTo > 0.0f;
    if (bypassFading)
        for (int ch = 0; ch < numChannels; ++ch)
            bypassBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
#endif

    // --- parametri: valore delle rampe a fine pezzo ---
    const bool  eqMoving  = std::any_of (eqFreqS.begin(), eqFreqS.end(), [] (auto& s) { return s.isSmoothing(); })
                         || std::any_of (eqGainS.begin(), eqGainS.end(), [] (auto& s) { return s.isSmoothing(); });
    for (auto& s : eqFreqS) s.skip (numSamples);
    for (auto& s : eqGainS) s.skip (numSamples);
    if (eqMoving)
        updateEqCoefficients();

    const float lowFreq   = lowFreqS.skip (numSamples);
    const float midFrom   = midFromS.skip (numSamples);
    const float midTo     = juce::jmax (midToS.skip (numSamples), midFrom + 1.0f);  // banda mai invertita
    const float highFreq  = highFreqS.skip (numSamples);
    const float tone      = toneS.skip (numSamples);
    const float compFrom  = compS.getCurrentValue();
    const float comp      = compS.skip (numSamples);
    const float satFrom   = lowSatS.getCurrentValue();
    const float lowSat    = lowSatS.skip (numSamples);
    const float driveFrom = midDriveS.getCurrentValue();
    const float midDrive  = midDriveS.skip (numSamples);
    const float mixFrom   = midMixS.getCurrentValue();
    const float midMix    = midMixS.skip (numSamples);
    const float lowFrom   = lowOnS.getCurrentValue();
    const float lowOn     = lowOnS.skip (numSamples);
    const float midFromOn = midOnS.getCurrentValue();
    const float midOn     = midOnS.skip (numSamples);
    const float highFrom  = highOnS.getCurrentValue();
    const float highOn    = highOnS.skip (numSamples);

    const int   character = (int) pCharacter->load();
    const int   midChar   = (int) pMidChar->load();
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    const int   cabMode   = (int) pCab->load();
    const bool  tunerOn   = pTunerOn->load() > 0.5f;
#endif
    const float gateThLin = juce::Decibels::decibelsToGain (pGate->load());

    lowpass.setCutoffFrequency     (lowFreq);
    midHighpass.setCutoffFrequency (midFrom);
    midLowpass.setCutoffFrequency  (midTo);
    highpass.setCutoffFrequency    (highFreq);
    toneFilter.setCutoffFrequency  (tone);
    compressor.setThreshold (compThresholdDb (comp));
    compressor.setRatio (compRatio (comp));

    juce::dsp::AudioBlock<float> mainBlock (buffer);
    juce::dsp::ProcessContextReplacing<float> mainCtx (mainBlock);

    // --- input gain, poi meter e tuner sul segnale post-gain ---
    inputGain.process (mainCtx);

    // --- hard gate: sotto soglia il suono si ferma ---
    // Apre a threshold, chiude a threshold - 6 dB (isteresi anti-chattering)
    {
        const float closeThLin = gateThLin * 0.5f;   // -6 dB

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

            gateEnvelope = juce::jmax (peak, gateEnvelope * gateEnvCoeff);

            if (gateEnvelope > gateThLin)
            {
                gateIsOpen = true;
                gateHoldCount = gateHoldSamples;
            }
            else if (gateEnvelope < closeThLin)
            {
                if (gateHoldCount > 0)
                    --gateHoldCount;
                else
                    gateIsOpen = false;
            }

            const float target = gateIsOpen ? 1.0f : 0.0f;
            const float coeff  = target > gateGainState ? gateOpenCoeff : gateCloseCoeff;
            gateGainState = target + (gateGainState - target) * coeff;

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * gateGainState);
        }
    }

    accumulatePeak (inputPeak, buffer);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    if (tunerOn)
        pushTunerSamples (buffer);
#endif

    // --- dry tap ---
    dryWet.pushDrySamples (mainBlock);

    // --- split nei tre tap paralleli ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowBuffer.copyFrom  (ch, 0, buffer, ch, 0, numSamples);
        midBuffer.copyFrom  (ch, 0, buffer, ch, 0, numSamples);
        highBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> lowBlock  (lowBuffer.getArrayOfWritePointers(),
                                            (size_t) numChannels, (size_t) numSamples);
    juce::dsp::AudioBlock<float> midBlock  (midBuffer.getArrayOfWritePointers(),
                                            (size_t) numChannels, (size_t) numSamples);
    juce::dsp::AudioBlock<float> highBlock (highBuffer.getArrayOfWritePointers(),
                                            (size_t) numChannels, (size_t) numSamples);

    juce::dsp::ProcessContextReplacing<float> lowCtx  (lowBlock);
    juce::dsp::ProcessContextReplacing<float> midCtx  (midBlock);
    juce::dsp::ProcessContextReplacing<float> highCtx (highBlock);

    // Ogni banda si spegne e si riaccende con una dissolvenza; spenta del tutto
    // non viene elaborata.

    // --- LOW BAND ---
    if (lowFrom > 0.0f || lowOn > 0.0f)
    {
        lowpass.process (lowCtx);
        compressor.process (lowCtx);

        const float makeupFrom = compMakeupDb (compFrom);
        const float makeup     = compMakeupDb (comp);
        if (makeupFrom > 0.01f || makeup > 0.01f)
            lowBuffer.applyGainRamp (0, numSamples, juce::Decibels::decibelsToGain (makeupFrom),
                                                    juce::Decibels::decibelsToGain (makeup));

        if (satFrom > 0.0f || lowSat > 0.0f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* d = lowBuffer.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float s = satFrom == lowSat ? lowSat
                                  : satFrom + (lowSat - satFrom) * (float) (i + 1) / (float) numSamples;
                    const float satDrive = 1.0f + s * 4.0f;
                    const float x = d[i];
                    d[i] = x + s * (softSat (x * satDrive) - x);
                }
            }
        }

        lowGain.process (lowCtx);
        applyFade (lowBuffer, numSamples, lowFrom, lowOn);
    }
    else
    {
        lowBuffer.clear();
    }

    // --- MID BAND: filtri, distorsione opzionale, boost/cut ---
    if (midFromOn > 0.0f || midOn > 0.0f)
    {
        midHighpass.process (midCtx);
        midLowpass.process  (midCtx);

        if (mixFrom > 0.0f || midMix > 0.0f)
        {
            // drive e compensazione seguono la rampa; lo shaper entra ed esce in
            // dissolvenza invece di scattare a 0.01 dB
            const bool crossfade = mixFrom < 1.0f || midMix < 1.0f;
            if (crossfade)
                for (int ch = 0; ch < numChannels; ++ch)
                    midDryBuffer.copyFrom (ch, 0, midBuffer, ch, 0, numSamples);

            const float driveLinFrom = juce::Decibels::decibelsToGain (driveFrom);
            const float driveLin     = juce::Decibels::decibelsToGain (midDrive);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* d = midBuffer.getWritePointer (ch);
                midBuffer.applyGainRamp (ch, 0, numSamples, driveLinFrom, driveLin);
                applyShaper (d, numSamples, midChar);
                // compensa il boost di drive
                midBuffer.applyGainRamp (ch, 0, numSamples, 1.0f / std::sqrt (driveLinFrom),
                                                            1.0f / std::sqrt (driveLin));
                if (crossfade)
                {
                    const float* dry = midDryBuffer.getReadPointer (ch);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float m = mixFrom + (midMix - mixFrom) * (float) (i + 1) / (float) numSamples;
                        d[i] = dry[i] + m * (d[i] - dry[i]);
                    }
                }
            }
        }

        midGain.process (midCtx);
        applyFade (midBuffer, numSamples, midFromOn, midOn);
    }
    else
    {
        midBuffer.clear();
    }

    // --- HIGH BAND ---
    if (highFrom > 0.0f || highOn > 0.0f)
    {
        highpass.process (highCtx);
        driveGain.process (highCtx);

        for (int ch = 0; ch < numChannels; ++ch)
            applyShaper (highBuffer.getWritePointer (ch), numSamples, character);

        toneFilter.process (highCtx);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
        if (cabMode == 1)
        {
            convolution.process (highCtx);
            cabGain.process (highCtx);
        }
#endif

        highGain.process (highCtx);
        applyFade (highBuffer, numSamples, highFrom, highOn);
    }
    else
    {
        highBuffer.clear();
    }

    // --- somma dei tre tap ---
    for (int ch = 0; ch < numChannels; ++ch)
    {
        buffer.copyFrom (ch, 0, lowBuffer,  ch, 0, numSamples);
        buffer.addFrom  (ch, 0, midBuffer,  ch, 0, numSamples);
        buffer.addFrom  (ch, 0, highBuffer, ch, 0, numSamples);
    }

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    if (cabMode == 2)
    {
        convolution.process (mainCtx);
        cabGain.process (mainCtx);
    }
#endif

    // --- EQ post sul mix globale ---
    eqLo.process (mainCtx);
    eqM1.process (mainCtx);
    eqM2.process (mainCtx);
    eqHi.process (mainCtx);

    // --- master ---
    outputGain.process (mainCtx);
    dryWet.mixWetSamples (mainBlock);

#if PARALLAXSTYLE_ANAGRAM_BUILD
    if (bypassFading)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            const float* in = bypassBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float m = bypassFrom + (bypassTo - bypassFrom) * (float) (i + 1) / (float) numSamples;
                d[i] += m * (in[i] - d[i]);
            }
        }
    }
#endif

    accumulatePeak (outputPeak, buffer);
}

//==============================================================================
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
void ParallaxStyleProcessor::loadIR (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    convolution.loadImpulseResponse (file,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0,
                                     juce::dsp::Convolution::Normalise::yes);

    const juce::ScopedLock sl (irPathLock);
    irPath = file.getFullPathName();
}

juce::File ParallaxStyleProcessor::getIRFile() const
{
    const juce::ScopedLock sl (irPathLock);
    return irPath.isNotEmpty() ? juce::File (irPath) : juce::File();
}
#endif

//==============================================================================
void ParallaxStyleProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

//==============================================================================
bool ParallaxStyleProcessor::savePreset (const juce::File& file)
{
    auto state = apvts.copyState();
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    {
        const juce::ScopedLock sl (irPathLock);
        state.setProperty ("irPath", irPath, nullptr);
    }
#endif

    if (auto xml = state.createXml())
    {
        file.getParentDirectory().createDirectory();
        return xml->writeTo (file);
    }
    return false;
}

bool ParallaxStyleProcessor::loadPreset (const juce::File& file)
{
    if (auto xml = juce::parseXML (file))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
            const juce::String savedPath = state.getProperty ("irPath", "").toString();
#endif
            apvts.replaceState (state);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
            if (savedPath.isNotEmpty())
                loadIR (juce::File (savedPath));
#endif
            return true;
        }
    }
    return false;
}

//==============================================================================
void ParallaxStyleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
    {
        const juce::ScopedLock sl (irPathLock);
        state.setProperty ("irPath", irPath, nullptr);
    }
#endif
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ParallaxStyleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
#if ! PARALLAXSTYLE_ANAGRAM_BUILD
            const juce::String savedPath = state.getProperty ("irPath", "").toString();
#endif
            apvts.replaceState (state);

#if ! PARALLAXSTYLE_ANAGRAM_BUILD
            if (savedPath.isNotEmpty())
                loadIR (juce::File (savedPath));
#endif
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* ParallaxStyleProcessor::createEditor()
{
#if PARALLAXSTYLE_ANAGRAM_BUILD
    return nullptr; // Anagram non ha display server: nessuna GUI JUCE per questo target.
#else
    return new ParallaxStyleEditor (*this);
#endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParallaxStyleProcessor();
}
