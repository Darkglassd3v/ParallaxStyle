#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    pHighFreq  = apvts.getRawParameterValue ("highfreq");
    pComp      = apvts.getRawParameterValue ("comp");
    pLowSat    = apvts.getRawParameterValue ("lowsat");
    pLowLevel  = apvts.getRawParameterValue ("lowlevel");
    pDrive     = apvts.getRawParameterValue ("drive");
    pCharacter = apvts.getRawParameterValue ("character");
    pTone      = apvts.getRawParameterValue ("tone");
    pHighLevel = apvts.getRawParameterValue ("highlevel");
    pCab       = apvts.getRawParameterValue ("cab");
    pCabLevel  = apvts.getRawParameterValue ("cablevel");
    pBlend     = apvts.getRawParameterValue ("blend");
    pOutput    = apvts.getRawParameterValue ("output");
    pTunerOn   = apvts.getRawParameterValue ("tuneron");
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
        "Mid Gain", juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f));

    // HIGH: distorci da... in su (100 Hz = fuzz lanoso, 1k+ = definizione chirurgica)
    params.push_back (std::make_unique<P> (juce::ParameterID { "highfreq", 1 },
        "High Freq", juce::NormalisableRange<float> (100.0f, 2000.0f, 1.0f, 0.5f), 400.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "comp", 1 },
        "Low Comp", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "lowsat", 1 },
        "Low Sat", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "lowlevel", 1 },
        "Low Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "drive", 1 },
        "Drive", juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 18.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "character", 1 }, "Character",
        juce::StringArray { "Tube", "Rodent", "Fuzz", "Doom", "X" }, 0));

    params.push_back (std::make_unique<P> (juce::ParameterID { "tone", 1 },
        "Tone", juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.4f), 5000.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "highlevel", 1 },
        "High Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "cab", 1 }, "Cab IR",
        juce::StringArray { "Off", "High Band", "Full Mix" }, 0));

    params.push_back (std::make_unique<P> (juce::ParameterID { "cablevel", 1 },
        "IR Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "blend", 1 },
        "Blend", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "output", 1 },
        "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

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

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "tuneron", 1 }, "Tuner", true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "lowon", 1 }, "Low On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "midon", 1 }, "Mid On", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "highon", 1 }, "High On", true));

    return { params.begin(), params.end() };
}

//==============================================================================
void ParallaxStyleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    inputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.02);

    // Hard gate: costanti di tempo fisse tarate per basso
    gateEnvCoeff    = std::exp (-1.0f / (float) (0.030 * sampleRate));  // envelope release 30 ms
    gateOpenCoeff   = std::exp (-1.0f / (float) (0.0005 * sampleRate)); // apertura 0.5 ms
    gateCloseCoeff  = std::exp (-1.0f / (float) (0.020 * sampleRate));  // chiusura 20 ms
    gateHoldSamples = (int) (0.040 * sampleRate);                       // hold 40 ms
    gateEnvelope = 0.0f;
    gateGainState = 1.0f;
    gateHoldCount = 0;
    gateIsOpen = true;

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
    compressor.setAttack (10.0f);
    compressor.setRelease (120.0f);
    compressor.setRatio (4.0f);

    lowGain.prepare (spec);
    lowGain.setRampDurationSeconds (0.02);

    driveGain.prepare (spec);
    driveGain.setRampDurationSeconds (0.02);

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);

    highGain.prepare (spec);
    highGain.setRampDurationSeconds (0.02);

    convolution.prepare (spec);

    cabGain.prepare (spec);
    cabGain.setRampDurationSeconds (0.02);

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);

    dryWet.prepare (spec);
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);

    currentSampleRate = sampleRate;
    eqLo.prepare (spec);
    eqM1.prepare (spec);
    eqM2.prepare (spec);
    eqHi.prepare (spec);
    for (auto& c : eqCache) c = -1e9f;   // forza il primo update
    updateEqCoefficients();

    lowBuffer.setSize  ((int) spec.numChannels, samplesPerBlock);
    midBuffer.setSize  ((int) spec.numChannels, samplesPerBlock);
    highBuffer.setSize ((int) spec.numChannels, samplesPerBlock);

    tunerSampleRate = sampleRate / (double) tunerDecimation;
}

bool ParallaxStyleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
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
void ParallaxStyleProcessor::updateEqCoefficients()
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    constexpr float q = 0.8f;

    const float vals[8] = { pEqLoF->load(), pEqLoG->load(),
                            pEqM1F->load(), pEqM1G->load(),
                            pEqM2F->load(), pEqM2G->load(),
                            pEqHiF->load(), pEqHiG->load() };

    bool changed = false;
    for (int i = 0; i < 8; ++i)
        if (vals[i] != eqCache[i]) { changed = true; break; }

    if (! changed)
        return;

    for (int i = 0; i < 8; ++i) eqCache[i] = vals[i];

    // I make* riallocano: eseguiti solo quando un valore cambia davvero
    *eqLo.state = *Coeffs::makeLowShelf  (currentSampleRate, vals[0], q,
                                          juce::Decibels::decibelsToGain (vals[1]));
    *eqM1.state = *Coeffs::makePeakFilter (currentSampleRate, vals[2], q,
                                          juce::Decibels::decibelsToGain (vals[3]));
    *eqM2.state = *Coeffs::makePeakFilter (currentSampleRate, vals[4], q,
                                          juce::Decibels::decibelsToGain (vals[5]));
    *eqHi.state = *Coeffs::makeHighShelf (currentSampleRate, vals[6], q,
                                          juce::Decibels::decibelsToGain (vals[7]));
}

//==============================================================================
void ParallaxStyleProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // --- parametri ---
    const float lowFreq   = pLowFreq->load();
    const float midFrom   = pMidFrom->load();
    const float midTo     = juce::jmax (pMidTo->load(), midFrom + 1.0f);  // banda mai invertita
    const float highFreq  = pHighFreq->load();
    const float comp      = pComp->load();
    const float lowSat    = pLowSat->load();
    const int   character = (int) pCharacter->load();
    const int   cabMode   = (int) pCab->load();
    const bool  tunerOn   = pTunerOn->load() > 0.5f;
    const bool  lowOn     = pLowOn->load()  > 0.5f;
    const bool  midOn     = pMidOn->load()  > 0.5f;
    const bool  highOn    = pHighOn->load() > 0.5f;

    inputGain.setGainDecibels (pInput->load());
    const float gateThLin = juce::Decibels::decibelsToGain (pGate->load());
    cabGain.setGainDecibels (pCabLevel->load());
    lowpass.setCutoffFrequency     (lowFreq);
    midHighpass.setCutoffFrequency (midFrom);
    midLowpass.setCutoffFrequency  (midTo);
    highpass.setCutoffFrequency    (highFreq);
    compressor.setThreshold (juce::jmap (comp, 0.0f, 1.0f, 0.0f, -36.0f));
    lowGain.setGainDecibels  (pLowLevel->load());
    midGain.setGainDecibels  (pMidGain->load());
    driveGain.setGainDecibels (pDrive->load());
    toneFilter.setCutoffFrequency (pTone->load());
    highGain.setGainDecibels (pHighLevel->load());
    outputGain.setGainDecibels (pOutput->load());
    dryWet.setWetMixProportion (pBlend->load());

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

    if (tunerOn)
        pushTunerSamples (buffer);

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

    // --- LOW BAND ---
    if (lowOn)
    {
        lowpass.process (lowCtx);
        compressor.process (lowCtx);

        if (lowSat > 0.0f)
        {
            const float satDrive = 1.0f + lowSat * 4.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* d = lowBuffer.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float x = d[i];
                    d[i] = x + lowSat * (softSat (x * satDrive) - x);
                }
            }
        }

        lowGain.process (lowCtx);
    }
    else
    {
        lowBuffer.clear();
    }

    // --- MID BAND: boost/cut pulito ---
    if (midOn)
    {
        midHighpass.process (midCtx);
        midLowpass.process  (midCtx);
        midGain.process (midCtx);
    }
    else
    {
        midBuffer.clear();
    }

    // --- HIGH BAND ---
    if (highOn)
    {
        highpass.process (highCtx);
        driveGain.process (highCtx);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* d = highBuffer.getWritePointer (ch);
            switch (character)
            {
                case 0: for (int i = 0; i < numSamples; ++i) d[i] = shapeTube   (d[i]); break;
                case 1: for (int i = 0; i < numSamples; ++i) d[i] = shapeRodent (d[i]); break;
                case 2: for (int i = 0; i < numSamples; ++i) d[i] = shapeFuzz   (d[i]); break;
                case 3: for (int i = 0; i < numSamples; ++i) d[i] = shapeDoom   (d[i]); break;
                case 4: for (int i = 0; i < numSamples; ++i) d[i] = shapeX      (d[i]); break;
                default: break;
            }
        }

        toneFilter.process (highCtx);

        if (cabMode == 1)
        {
            convolution.process (highCtx);
            cabGain.process (highCtx);
        }

        highGain.process (highCtx);
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

    if (cabMode == 2)
    {
        convolution.process (mainCtx);
        cabGain.process (mainCtx);
    }

    // --- EQ post sul mix globale ---
    updateEqCoefficients();
    eqLo.process (mainCtx);
    eqM1.process (mainCtx);
    eqM2.process (mainCtx);
    eqHi.process (mainCtx);

    // --- master ---
    outputGain.process (mainCtx);
    dryWet.mixWetSamples (mainBlock);

    accumulatePeak (outputPeak, buffer);
}

//==============================================================================
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

//==============================================================================
bool ParallaxStyleProcessor::savePreset (const juce::File& file)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (irPathLock);
        state.setProperty ("irPath", irPath, nullptr);
    }

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
            const juce::String savedPath = state.getProperty ("irPath", "").toString();
            apvts.replaceState (state);

            if (savedPath.isNotEmpty())
                loadIR (juce::File (savedPath));
            return true;
        }
    }
    return false;
}

//==============================================================================
void ParallaxStyleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (irPathLock);
        state.setProperty ("irPath", irPath, nullptr);
    }
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
            const juce::String savedPath = state.getProperty ("irPath", "").toString();
            apvts.replaceState (state);

            if (savedPath.isNotEmpty())
                loadIR (juce::File (savedPath));
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* ParallaxStyleProcessor::createEditor()
{
    return new ParallaxStyleEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParallaxStyleProcessor();
}
